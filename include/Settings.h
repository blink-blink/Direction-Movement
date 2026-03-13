#include "SKSEMCP/SKSEMenuFramework.hpp"
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

// RapidJSON headers
#include <rapidjson/document.h>
#include <rapidjson/istreamwrapper.h>
#include <rapidjson/ostreamwrapper.h>
#include <rapidjson/prettywriter.h>
#include <rapidjson/stringbuffer.h> 

// Miniz (Zip)
#include <miniz.h>

namespace OARConverterUI {

    namespace fs = std::filesystem;

    // Estrutura para segurar o conteúdo do arquivo na memória antes de exportar
    struct ConvertedFile {
        fs::path originalPath;
        std::string modifiedContent;
    };

    std::wstring ToLowerW(std::wstring s) {
        std::transform(s.begin(), s.end(), s.begin(), ::towlower);
        return s;
    }

    // 1. Extração do Direcional (Retorna 0 se não for Keytrace, ou o número da direção 1, 3, 5, 7)
    struct KeytraceInfo {
        bool isValid = false;
        bool isDirectional = false;
        int dirValue = 0;
        std::string boolVarName = "";
    };

    KeytraceInfo ParseKeytraceCondition(const rapidjson::Value& cond) {
        KeytraceInfo info;
        if (!cond.IsObject()) return info;
        if (!cond.HasMember("condition") || !cond["condition"].IsString()) return info;

        std::string conditionType = cond["condition"].GetString();
        if (conditionType != "HasMagicEffect") return info;

        const rapidjson::Value* me = nullptr;
        if (cond.HasMember("Magic Effect")) me = &cond["Magic Effect"];
        else if (cond.HasMember("Magic effect")) me = &cond["Magic effect"];
        else if (cond.HasMember("magicEffect")) me = &cond["magicEffect"];
        else if (cond.HasMember("Effect")) me = &cond["Effect"];
        else if (cond.HasMember("effect")) me = &cond["effect"];

        if (!me || !me->IsObject()) return info;

        if (me->HasMember("pluginName") && (*me)["pluginName"].IsString() && me->HasMember("formID")) {
            std::string pluginName = (*me)["pluginName"].GetString();
            std::transform(pluginName.begin(), pluginName.end(), pluginName.begin(), ::tolower);

            if (pluginName == "keytrace.esp") {
                std::string formID = "";
                if ((*me)["formID"].IsString()) {
                    formID = (*me)["formID"].GetString();
                }
                else if ((*me)["formID"].IsInt()) {
                    char hexStr[20];
                    sprintf_s(hexStr, "%X", (*me)["formID"].GetInt());
                    formID = hexStr;
                }

                // Tudo em minusculo para garantir que acha o Hex corretamente
                std::transform(formID.begin(), formID.end(), formID.begin(), ::tolower);
                info.isValid = true;

                // Direcionais Antigos (Int)
                if (formID.find("801") != std::string::npos) { info.isDirectional = true; info.dirValue = 1; }
                else if (formID.find("803") != std::string::npos) { info.isDirectional = true; info.dirValue = 5; }
                else if (formID.find("802") != std::string::npos) { info.isDirectional = true; info.dirValue = 7; }
                else if (formID.find("804") != std::string::npos) { info.isDirectional = true; info.dirValue = 3; }
                // Novas Teclas (Bools)
                else if (formID.find("81b") != std::string::npos) { info.boolVarName = "DMKLeftShift"; }
                else if (formID.find("81c") != std::string::npos) { info.boolVarName = "DMKQ"; }
                else if (formID.find("81d") != std::string::npos) { info.boolVarName = "DMKE"; }
                else if (formID.find("81e") != std::string::npos) { info.boolVarName = "DMKLeftAlt"; }
                else if (formID.find("821") != std::string::npos) { info.boolVarName = "DMKZ"; }
                else if (formID.find("822") != std::string::npos) { info.boolVarName = "DMKX"; }
                else { info.isValid = false; }
            }
        }
        return info;
    }

    // 2. Helper para combinar direções em uma Diagonal (Padrão 8-Way)
    int CombineDirections(int dir1, int dir2) {
        // Diagonais padrão (8-Way)
        if ((dir1 == 1 && dir2 == 3) || (dir1 == 3 && dir2 == 1)) return 2; // Frente-Direita
        if ((dir1 == 5 && dir2 == 3) || (dir1 == 3 && dir2 == 5)) return 4; // Trás-Direita
        if ((dir1 == 5 && dir2 == 7) || (dir1 == 7 && dir2 == 5)) return 6; // Trás-Esquerda
        if ((dir1 == 1 && dir2 == 7) || (dir1 == 7 && dir2 == 1)) return 8; // Frente-Esquerda

        // Teclas Opostas (Novo)
        if ((dir1 == 1 && dir2 == 5) || (dir1 == 5 && dir2 == 1)) return 9; // Frente-Trás
        if ((dir1 == 7 && dir2 == 3) || (dir1 == 3 && dir2 == 7)) return 10; // Esquerda-Direita

        return dir1; // Retorno de segurança
    }

    // 3. Monta a nova estrutura do JSON substituindo a antiga
    void ReplaceWithNewCondition(rapidjson::Value& cond, const KeytraceInfo& info, bool isNegated, rapidjson::Document::AllocatorType& allocator) {
        cond.RemoveAllMembers();

        cond.AddMember("condition", "CompareValues", allocator);
        cond.AddMember("negated", isNegated, allocator);
        cond.AddMember("requiredVersion", "1.0.0.0", allocator);

        if (info.isDirectional) {
            rapidjson::Value valA(rapidjson::kObjectType);
            valA.AddMember("value", info.dirValue, allocator);
            cond.AddMember("Value A", valA, allocator);

            cond.AddMember("Comparison", "==", allocator);

            rapidjson::Value valB(rapidjson::kObjectType);
            valB.AddMember("graphVariable", "DirecionalCycleMoveset", allocator);
            valB.AddMember("graphVariableType", "Int", allocator);
            cond.AddMember("Value B", valB, allocator);
        }
        else {
            // Se for bool, preenche como "True" / "Bool"
            rapidjson::Value valA(rapidjson::kObjectType);
            valA.AddMember("value", 1, allocator);
            cond.AddMember("Value A", valA, allocator);

            cond.AddMember("Comparison", "==", allocator);

            rapidjson::Value valB(rapidjson::kObjectType);
            rapidjson::Value stringVarName;
            stringVarName.SetString(info.boolVarName.c_str(), allocator);
            valB.AddMember("graphVariable", stringVarName, allocator);
            valB.AddMember("graphVariableType", "Bool", allocator);
            cond.AddMember("Value B", valB, allocator);
        }
    }

    // 4. Busca recursivamente, avalia arrays e junta as diagonais se necessário
// 4. Busca recursivamente, avalia arrays e junta as diagonais se necessário
    void TraverseAndReplace(rapidjson::Value& node, rapidjson::Document::AllocatorType& allocator, int& modifiedCount, bool isAndContext = true) {

        // --- NOVIDADE: Navegação em Objetos (Nós Raiz, AND e OR) ---
        if (node.IsObject()) {
            bool currentContextIsAnd = isAndContext;

            // Verifica se este objeto altera o contexto lógico (ex: nó "OR" não pode somar combos)
            if (node.HasMember("condition") && node["condition"].IsString()) {
                std::string cType = node["condition"].GetString();
                if (cType == "OR" || cType == "XOR") {
                    currentContextIsAnd = false;
                }
                else if (cType == "AND") {
                    currentContextIsAnd = true;
                }
            }

            // Percorre todos os membros do Objeto procurando chaves que contenham Arrays
            for (auto itr = node.MemberBegin(); itr != node.MemberEnd(); ++itr) {
                // Se o membro for a lista de condições ("conditions" ou "Conditions")
                if (itr->name == "conditions" || itr->name == "Conditions") {
                    TraverseAndReplace(itr->value, allocator, modifiedCount, currentContextIsAnd);
                }
                else {
                    // Repassa a recursividade para as outras chaves do objeto
                    TraverseAndReplace(itr->value, allocator, modifiedCount, isAndContext);
                }
            }
        }

        // --- CÓDIGO DE ARRAYS (Onde a conversão realmente acontece) ---
        else if (node.IsArray()) {
            std::vector<rapidjson::SizeType> positiveDirIndices;
            std::vector<int> positiveDirValues;

            std::vector<rapidjson::SizeType> negatedDirIndices;
            std::vector<int> negatedDirValues;

            std::vector<rapidjson::SizeType> indicesToErase;

            // Etapa A: Separar Direcionais Positivos e Negados
            for (rapidjson::SizeType i = 0; i < node.Size(); i++) {
                KeytraceInfo info = ParseKeytraceCondition(node[i]);
                if (info.isValid && info.isDirectional) {
                    bool isNegated = false;
                    if (node[i].HasMember("negated") && node[i]["negated"].IsBool()) {
                        isNegated = node[i]["negated"].GetBool();
                    }

                    if (isNegated) {
                        negatedDirIndices.push_back(i);
                        negatedDirValues.push_back(info.dirValue);
                    }
                    else {
                        positiveDirIndices.push_back(i);
                        positiveDirValues.push_back(info.dirValue);
                    }
                }
            }

            // Etapa B: Filtrar Duplicatas e Agrupar APENAS os Positivos
            std::vector<int> uniqueDirValues;
            std::vector<rapidjson::SizeType> uniqueDirIndices;

            // Limpa modders que colocaram a mesma direção duas vezes no arquivo
            for (size_t i = 0; i < positiveDirIndices.size(); i++) {
                int val = positiveDirValues[i];
                if (std::find(uniqueDirValues.begin(), uniqueDirValues.end(), val) == uniqueDirValues.end()) {
                    uniqueDirValues.push_back(val);
                    uniqueDirIndices.push_back(positiveDirIndices[i]);
                }
                else {
                    // É uma duplicata inútil, já marcamos para deletar do JSON
                    indicesToErase.push_back(positiveDirIndices[i]);
                }
            }

            bool grouped = false; // Flag de agrupamento

            if (isAndContext && uniqueDirValues.size() >= 2) {
                if (uniqueDirValues.size() == 3) {
                    int sum = uniqueDirValues[0] + uniqueDirValues[1] + uniqueDirValues[2];
                    int combinedDir = 0;

                    if (sum == 11) combinedDir = 11;      // Frente(1) + Esquerda(7) + Direita(3)
                    else if (sum == 9) combinedDir = 12;  // Frente(1) + Trás(5) + Direita(3)
                    else if (sum == 15) combinedDir = 13; // Trás(5) + Esquerda(7) + Direita(3)
                    else if (sum == 13) combinedDir = 14; // Frente(1) + Trás(5) + Esquerda(7)

                    if (combinedDir != 0) {
                        KeytraceInfo comboInfo;
                        comboInfo.isDirectional = true;
                        comboInfo.dirValue = combinedDir;

                        ReplaceWithNewCondition(node[uniqueDirIndices[0]], comboInfo, false, allocator);
                        modifiedCount++;

                        indicesToErase.push_back(uniqueDirIndices[1]);
                        indicesToErase.push_back(uniqueDirIndices[2]);
                        grouped = true;
                    }
                }
                else if (uniqueDirValues.size() == 2) {
                    int combinedDir = CombineDirections(uniqueDirValues[0], uniqueDirValues[1]);

                    KeytraceInfo comboInfo;
                    comboInfo.isDirectional = true;
                    comboInfo.dirValue = combinedDir;

                    ReplaceWithNewCondition(node[uniqueDirIndices[0]], comboInfo, false, allocator);
                    modifiedCount++;

                    indicesToErase.push_back(uniqueDirIndices[1]);
                    grouped = true;
                }
            }

            // Fallback: Se não agrupou, ou é só uma tecla, converte individualmente
            if (!grouped && !uniqueDirIndices.empty()) {
                for (size_t i = 0; i < uniqueDirIndices.size(); i++) {
                    KeytraceInfo dInfo;
                    dInfo.isDirectional = true;
                    dInfo.dirValue = uniqueDirValues[i];
                    ReplaceWithNewCondition(node[uniqueDirIndices[i]], dInfo, false, allocator);
                    modifiedCount++;
                }
            }

            // Etapa C: Processar os NEGADOS sempre individualmente
            for (size_t i = 0; i < negatedDirIndices.size(); i++) {
                KeytraceInfo dInfo;
                dInfo.isDirectional = true;
                dInfo.dirValue = negatedDirValues[i];
                ReplaceWithNewCondition(node[negatedDirIndices[i]], dInfo, true, allocator);
                modifiedCount++;
            }

            // Etapa D: Processar as Chaves Adicionais (Esquiva/Ataque etc)
            for (rapidjson::SizeType i = 0; i < node.Size(); i++) {
                if (std::find(indicesToErase.begin(), indicesToErase.end(), i) != indicesToErase.end()) continue;

                KeytraceInfo info = ParseKeytraceCondition(node[i]);
                if (info.isValid && !info.isDirectional) {
                    bool isNegated = false;
                    if (node[i].HasMember("negated") && node[i]["negated"].IsBool()) {
                        isNegated = node[i]["negated"].GetBool();
                    }
                    ReplaceWithNewCondition(node[i], info, isNegated, allocator);
                    modifiedCount++;
                }
            }

            // Etapa E: Continuar recursividade nos filhos restantes
            for (rapidjson::SizeType i = 0; i < node.Size(); i++) {
                if (std::find(indicesToErase.begin(), indicesToErase.end(), i) != indicesToErase.end()) continue;
                TraverseAndReplace(node[i], allocator, modifiedCount, isAndContext);
            }

            // Etapa F: Apagar do JSON as redundâncias e duplicatas
            std::sort(indicesToErase.rbegin(), indicesToErase.rend());
            for (rapidjson::SizeType idx : indicesToErase) {
                node.Erase(node.Begin() + idx);
            }
        }
    }

    // --- HELPER: Converte o caminho para UTF-8 de forma segura para usar no log do SKSE ---
    std::string PathToLogString(const fs::path& p) {
        try {
            auto u8 = p.u8string();
            return std::string(reinterpret_cast<const char*>(u8.c_str()));
        }
        catch (...) {
            return "Caminho com caracteres ilegiveis";
        }
    }

    // 5. Processamento do arquivo OAR (Agora armazena na memória antes de escrever no disco)
    void ProcessOARJson(const fs::path& filepath, int& totalFilesModified, int& totalConditionsModified, int& totalScanned, std::vector<ConvertedFile>& modifiedFilesList, bool exportToZip) {
        std::string logPath = PathToLogString(filepath);
        totalScanned++; // Incrementa contador de arquivos lidos

        try {
            std::ifstream ifs(filepath);
            if (!ifs.is_open()) return;

            rapidjson::IStreamWrapper isw(ifs);
            rapidjson::Document doc;
            doc.ParseStream(isw);
            ifs.close();

            if (doc.HasParseError() || !doc.IsObject()) return;

            int modifiedInThisFile = 0;
            // A chamada inicial começa o arquivo considerando que está tudo num contexto "AND"
            TraverseAndReplace(doc, doc.GetAllocator(), modifiedInThisFile, true);

            if (modifiedInThisFile > 0) {

                // Grava o JSON modificado em uma string na memória
                rapidjson::StringBuffer buffer;
                rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(buffer);
                doc.Accept(writer);
                std::string modifiedJsonString = buffer.GetString();

                // SE NÃO FOR EXPORTAR PARA ZIP, SOBRESCREVE O ARQUIVO ORIGINAL
                if (!exportToZip) {
                    std::ofstream ofs(filepath);
                    if (ofs.is_open()) {
                        ofs << modifiedJsonString;
                        ofs.close();
                    }
                }

                totalFilesModified++;
                totalConditionsModified += modifiedInThisFile;

                // Adiciona na nossa lista de arquivos (com o conteúdo) para a criação do ZIP
                modifiedFilesList.push_back({ filepath, modifiedJsonString });

                SKSE::log::info("Sucesso! Processado: {} ({} condicoes alteradas)", logPath, modifiedInThisFile);
            }
        }
        catch (const std::exception& e) {
            SKSE::log::error("Erro no arquivo [{}]: {}", logPath, e.what());
        }
    }

    // 6. Varredura de pastas Atualizada (Recebe a flag 'exportToZip')
    void RunConversionProcess(std::vector<ConvertedFile>& modifiedFilesList, bool exportToZip) {
        SKSE::log::info("Iniciando escaneamento para conversao de condicoes OAR...");
        try {
            fs::path startPath = "Data/meshes";
            if (!fs::exists(startPath)) {
                SKSE::log::warn("A pasta Data/meshes nao foi encontrada.");
                return;
            }

            int totalFiles = 0;
            int totalConditions = 0;
            int totalScanned = 0;

            for (const auto& entry : fs::recursive_directory_iterator(startPath)) {
                if (entry.is_regular_file()) {

                    // Pega o nome do arquivo em minúsculas
                    std::wstring filename = ToLowerW(entry.path().filename().wstring());

                    // Modificação: Removida a trava da pasta "openanimationreplacer"
                    // Agora varre qualquer config.json ou user.json encontrado
                    if (filename == L"config.json" || filename == L"user.json") {
                        ProcessOARJson(entry.path(), totalFiles, totalConditions, totalScanned, modifiedFilesList, exportToZip);
                    }
                }
            }

            SKSE::log::info("=== CONVERSAO OAR CONCLUIDA ===");
            SKSE::log::info("Arquivos config/user.json lidos: {}", totalScanned);
            SKSE::log::info("Arquivos processados: {}", totalFiles);
            SKSE::log::info("Condicoes substituidas: {}", totalConditions);
        }
        catch (const std::filesystem::filesystem_error& e) {
            SKSE::log::critical("Erro de sistema de arquivos: {}", e.what());
        }
        catch (const std::exception& e) {
            SKSE::log::critical("Erro fatal durante a varredura: {}", e.what());
        }
    }

    // 7. Salva os arquivos modificados em um ZIP (Escrevendo direto da memória para o ZIP)
    void ExportConvertedFilesToZip(const std::vector<ConvertedFile>& convertedFiles) {
        if (convertedFiles.empty()) {
            return;
        }

        fs::path exportDir = "Data/export";
        fs::create_directories(exportDir); // Garante que a pasta existe

        std::string zipPath = (exportDir / "DirectionalConverted.zip").string();

        mz_zip_archive zip_archive;
        memset(&zip_archive, 0, sizeof(zip_archive));

        if (!mz_zip_writer_init_file(&zip_archive, zipPath.c_str(), 0)) {
            SKSE::log::error("Export OAR: Falha ao inicializar arquivo ZIP em {}", zipPath);
            return;
        }

        for (const auto& file : convertedFiles) {
            std::string sourcePath = file.originalPath.string();

            // O arquivo já deve ter um path como "Data\meshes\...", ajustamos apenas as barras
            std::string internalZipPath = sourcePath;
            std::replace(internalZipPath.begin(), internalZipPath.end(), '\\', '/');

            // USANDO mz_zip_writer_add_mem PARA LER A STRING DA MEMÓRIA DIRETO PARA O ZIP
            if (!mz_zip_writer_add_mem(&zip_archive, internalZipPath.c_str(), file.modifiedContent.data(), file.modifiedContent.size(), MZ_BEST_COMPRESSION)) {
                SKSE::log::error("Export OAR: Falha ao adicionar arquivo {} ao ZIP", internalZipPath);
            }
            else {
                SKSE::log::info("Export OAR: Adicionado ao ZIP: {}", internalZipPath);
            }
        }

        mz_zip_writer_finalize_archive(&zip_archive);
        mz_zip_writer_end(&zip_archive);

        SKSE::log::info("Exportacao OAR concluida com sucesso para: {}", zipPath);
    }

    // --- RENDERIZAÇÃO DA UI NO SKSE MENU FRAMEWORK ---
    void RenderMenu() {
        static bool showConfirmPopup = false;
        static bool showSuccessMessage = false;
        static bool exportToZip = true; // Opção adicionada e ativa por padrão

        ImGuiMCP::Text("OAR Conditions Converter");
        ImGuiMCP::Separator();
        ImGuiMCP::Spacing();

        ImGuiMCP::TextWrapped("It finds Dtry conditions (Keytrace.esp directional magic effects)");
        ImGuiMCP::TextWrapped("and replaces them with the new DirecionalCycleMoveset Graph Variable.");
        ImGuiMCP::Spacing();

        // Checkbox para o usuário decidir se vai exportar também para ZIP
        ImGuiMCP::Checkbox("Export converted files to ZIP (Do NOT touch original files)", &exportToZip);
        ImGuiMCP::Spacing();

        if (ImGuiMCP::Button("Converter Dtry conditions", { 250, 40 })) {
            showConfirmPopup = true;
            showSuccessMessage = false;
        }

        if (showSuccessMessage) {
            ImGuiMCP::Spacing();
            ImGuiMCP::TextColored({ 0.4f, 1.0f, 0.4f, 1.0f }, "Conversion completed successfully! Check SKSE logs for details.");
            if (exportToZip) {
                ImGuiMCP::TextColored({ 0.4f, 1.0f, 0.4f, 1.0f }, "Zip created at Data/export/OAR_Converted_Export.zip (Original files untouched)");
            }
        }

        // --- LÓGICA DO POPUP DE CONFIRMAÇÃO ---
        if (showConfirmPopup) {
            ImGuiMCP::OpenPopup("Confirm Conversion");
        }

        // Centraliza o popup
        if (ImGuiMCP::BeginPopupModal("Confirm Conversion", nullptr, ImGuiMCP::ImGuiWindowFlags_AlwaysAutoResize)) {

            if (exportToZip) {
                ImGuiMCP::Text("Are you sure you want to extract the conditions?");
                ImGuiMCP::Text("Original files will NOT be modified. A ZIP file will be created.");
            }
            else {
                ImGuiMCP::Text("Are you sure you want to convert the conditions?");
                ImGuiMCP::Text("This change is permanent. Your original files WILL BE MODIFIED.");
            }

            ImGuiMCP::Text("You will need restart the game after finished");
            ImGuiMCP::Spacing();
            ImGuiMCP::Separator();
            ImGuiMCP::Spacing();

            if (ImGuiMCP::Button("Yes, Convert", { 120, 0 })) {

                std::vector<ConvertedFile> modifiedFiles; // Agora usa o struct com a string

                // Passa a flag exportToZip para evitar alterar os originais se marcada
                RunConversionProcess(modifiedFiles, exportToZip);

                // Se a opção estiver marcada e arquivos foram modificados, cria o ZIP
                if (exportToZip && !modifiedFiles.empty()) {
                    ExportConvertedFilesToZip(modifiedFiles);
                }

                showConfirmPopup = false;
                showSuccessMessage = true;
                ImGuiMCP::CloseCurrentPopup();
            }

            ImGuiMCP::SameLine();

            if (ImGuiMCP::Button("Cancel", { 120, 0 })) {
                showConfirmPopup = false;
                ImGuiMCP::CloseCurrentPopup();
            }
            ImGuiMCP::EndPopup();
        }
    }

    // Registro do menu
    void Register() {
        if (!SKSEMenuFramework::IsInstalled()) {
            return;
        }
        SKSEMenuFramework::SetSection("Directional Movement");
        SKSEMenuFramework::AddSectionItem("Legacy Converter", RenderMenu);
        SKSE::log::info("OAR Converter UI Registered successfully.");
    }
}