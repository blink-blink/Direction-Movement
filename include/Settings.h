#include "SKSEMCP/SKSEMenuFramework.hpp"
#include <filesystem>
#include <fstream>
#include <string>

// RapidJSON headers
#include <rapidjson/document.h>
#include <rapidjson/istreamwrapper.h>
#include <rapidjson/ostreamwrapper.h>
#include <rapidjson/prettywriter.h>

namespace OARConverterUI {

    namespace fs = std::filesystem;

    std::wstring ToLowerW(std::wstring s) {
        std::transform(s.begin(), s.end(), s.begin(), ::towlower);
        return s;
    }

    // 1. Extração do Direcional (Retorna 0 se não for Keytrace, ou o número da direção 1, 3, 5, 7)
    int GetKeytraceDirection(const rapidjson::Value& cond) {
        if (!cond.IsObject()) return 0;
        if (!cond.HasMember("condition") || !cond["condition"].IsString()) return 0;

        std::string conditionType = cond["condition"].GetString();
        if (conditionType != "HasMagicEffect") return 0;

        // Tenta encontrar o campo do Magic Effect (Variações comuns no OAR)
        const rapidjson::Value* me = nullptr;
        if (cond.HasMember("Magic Effect")) me = &cond["Magic Effect"];
        else if (cond.HasMember("Magic effect")) me = &cond["Magic effect"];
        else if (cond.HasMember("magicEffect")) me = &cond["magicEffect"];
        else if (cond.HasMember("Effect")) me = &cond["Effect"];
        else if (cond.HasMember("effect")) me = &cond["effect"];

        if (!me || !me->IsObject()) return 0;

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

                if (formID.find("801") != std::string::npos) return 1; // Frente
                if (formID.find("803") != std::string::npos) return 5; // Trás
                if (formID.find("802") != std::string::npos) return 7; // Esquerda
                if (formID.find("804") != std::string::npos) return 3; // Direita
            }
        }
        return 0;
    }

    // 2. Helper para combinar direções em uma Diagonal (Padrão 8-Way)
    int CombineDirections(int dir1, int dir2) {
        if ((dir1 == 1 && dir2 == 3) || (dir1 == 3 && dir2 == 1)) return 2; // Frente-Direita
        if ((dir1 == 5 && dir2 == 3) || (dir1 == 3 && dir2 == 5)) return 4; // Trás-Direita
        if ((dir1 == 5 && dir2 == 7) || (dir1 == 7 && dir2 == 5)) return 6; // Trás-Esquerda
        if ((dir1 == 1 && dir2 == 7) || (dir1 == 7 && dir2 == 1)) return 8; // Frente-Esquerda
        return dir1; // Retorno de segurança
    }

    // 3. Monta a nova estrutura do JSON substituindo a antiga
    void ReplaceWithNewCondition(rapidjson::Value& cond, int directionValue, bool isNegated, rapidjson::Document::AllocatorType& allocator) {
        cond.RemoveAllMembers(); // Limpa a condição antiga

        cond.AddMember("condition", "CompareValues", allocator);
        cond.AddMember("negated", isNegated, allocator); // <--- Agora usa o valor original
        cond.AddMember("requiredVersion", "1.0.0.0", allocator);

        rapidjson::Value valA(rapidjson::kObjectType);
        valA.AddMember("value", directionValue, allocator);
        cond.AddMember("Value A", valA, allocator);

        cond.AddMember("Comparison", "==", allocator);

        rapidjson::Value valB(rapidjson::kObjectType);
        valB.AddMember("graphVariable", "DirecionalCycleMoveset", allocator);
        valB.AddMember("graphVariableType", "Int", allocator);
        cond.AddMember("Value B", valB, allocator);
    }

    // 4. Busca recursivamente, avalia arrays e junta as diagonais se necessário
    void TraverseAndReplace(rapidjson::Value& node, rapidjson::Document::AllocatorType& allocator, int& modifiedCount, bool isAndContext = true) {
        if (node.IsArray()) {
            std::vector<rapidjson::SizeType> keytraceIndices;
            std::vector<int> keytraceDirections;

            // Coleta todas as condições do Keytrace que são "irmãs" neste mesmo array
            for (rapidjson::SizeType i = 0; i < node.Size(); i++) {
                int dir = GetKeytraceDirection(node[i]);
                if (dir > 0) {
                    keytraceIndices.push_back(i);
                    keytraceDirections.push_back(dir);
                }
            }

            // Se acharmos exatamente 2 direções num contexto "AND", formamos uma diagonal
            if (isAndContext && keytraceIndices.size() == 2) {
                int combinedDir = CombineDirections(keytraceDirections[0], keytraceDirections[1]);
                SKSE::log::trace("  [DEBUG] DIAGONAL DETECTADA: Direcoes ({}, {}) combinadas para -> {}", keytraceDirections[0], keytraceDirections[1], combinedDir);

                // Verifica se a primeira condição da diagonal era negada
                bool isNegated = false;
                if (node[keytraceIndices[0]].HasMember("negated") && node[keytraceIndices[0]]["negated"].IsBool()) {
                    isNegated = node[keytraceIndices[0]]["negated"].GetBool();
                }

                // Substitui o primeiro bloco pela variável diagonal mantendo o negated
                ReplaceWithNewCondition(node[keytraceIndices[0]], combinedDir, isNegated, allocator);
                modifiedCount++;

                // APAGA o segundo bloco para que não haja conflito
                node.Erase(node.Begin() + keytraceIndices[1]);
            }
            else {
                // Caso seja contexto OR ou seja apenas 1 direção individual, substitui normal
                for (size_t i = 0; i < keytraceIndices.size(); i++) {

                    // Extrai a flag "negated" da condição individual antes de a alterar
                    bool isNegated = false;
                    if (node[keytraceIndices[i]].HasMember("negated") && node[keytraceIndices[i]]["negated"].IsBool()) {
                        isNegated = node[keytraceIndices[i]]["negated"].GetBool();
                    }

                    ReplaceWithNewCondition(node[keytraceIndices[i]], keytraceDirections[i], isNegated, allocator);
                    modifiedCount++;
                }
            }

            // Continua a recursividade em todos os elementos restantes do array
            for (rapidjson::SizeType i = 0; i < node.Size(); i++) {
                TraverseAndReplace(node[i], allocator, modifiedCount, isAndContext);
            }
        }
        else if (node.IsObject()) {
            bool nextContextIsAnd = isAndContext;

            if (node.HasMember("condition") && node["condition"].IsString()) {
                std::string condType = node["condition"].GetString();
                if (condType == "OR") {
                    nextContextIsAnd = false;
                }
                else if (condType == "AND") {
                    nextContextIsAnd = true;
                }
            }

            for (auto itr = node.MemberBegin(); itr != node.MemberEnd(); ++itr) {
                if (std::string(itr->name.GetString()) == "conditions") {
                    TraverseAndReplace(itr->value, allocator, modifiedCount, true);
                }
                else {
                    TraverseAndReplace(itr->value, allocator, modifiedCount, nextContextIsAnd);
                }
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

    // 5. Processamento do arquivo OAR
    void ProcessOARJson(const fs::path& filepath, int& totalFilesModified, int& totalConditionsModified, int& totalScanned) {
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
                std::ofstream ofs(filepath);
                if (ofs.is_open()) {
                    rapidjson::OStreamWrapper osw(ofs);
                    rapidjson::PrettyWriter<rapidjson::OStreamWrapper> writer(osw);
                    doc.Accept(writer);
                    ofs.close();

                    totalFilesModified++;
                    totalConditionsModified += modifiedInThisFile;
                    SKSE::log::info("Sucesso! Atualizado: {} ({} condicoes alteradas)", logPath, modifiedInThisFile);
                }
            }
        }
        catch (const std::exception& e) {
            SKSE::log::error("Erro no arquivo [{}]: {}", logPath, e.what());
        }
    }
    // 3. Varredura de pastas Atualizada (Busca Case-Insensitive em QUALQUER pasta dentro de meshes)
    void RunConversionProcess() {
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
                        ProcessOARJson(entry.path(), totalFiles, totalConditions, totalScanned);
                    }
                }
            }

            SKSE::log::info("=== CONVERSAO OAR CONCLUIDA ===");
            SKSE::log::info("Arquivos config/user.json lidos: {}", totalScanned);
            SKSE::log::info("Arquivos modificados: {}", totalFiles);
            SKSE::log::info("Condicoes substituidas: {}", totalConditions);
        }
        catch (const std::filesystem::filesystem_error& e) {
            SKSE::log::critical("Erro de sistema de arquivos: {}", e.what());
        }
        catch (const std::exception& e) {
            SKSE::log::critical("Erro fatal durante a varredura: {}", e.what());
        }
    }

    // --- RENDERIZAÇÃO DA UI NO SKSE MENU FRAMEWORK ---
    void RenderMenu() {
        static bool showConfirmPopup = false;
        static bool showSuccessMessage = false;

        ImGuiMCP::Text("OAR Conditions Converter");
        ImGuiMCP::Separator();
        ImGuiMCP::Spacing();

        ImGuiMCP::TextWrapped("It finds Dtry conditions (Keytrace.esp directional magic effects)");
        ImGuiMCP::TextWrapped("and replaces them with the new DirecionalCycleMoveset Graph Variable.");
        ImGuiMCP::Spacing();

        if (ImGuiMCP::Button("Converter Dtry conditions", { 250, 40 })) {
            showConfirmPopup = true;
            showSuccessMessage = false;
        }

        if (showSuccessMessage) {
            ImGuiMCP::Spacing();
            ImGuiMCP::TextColored({ 0.4f, 1.0f, 0.4f, 1.0f }, "Conversion completed successfully! Check SKSE logs for details.");
        }

        // --- LÓGICA DO POPUP DE CONFIRMAÇÃO ---
        if (showConfirmPopup) {
            ImGuiMCP::OpenPopup("Confirm Conversion");
        }

        // Centraliza o popup
        if (ImGuiMCP::BeginPopupModal("Confirm Conversion", nullptr, ImGuiMCP::ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGuiMCP::Text("Are you sure you want to convert the conditions?");
            ImGuiMCP::Text("This change is permanently.");
            ImGuiMCP::Text("You will need restart the game after finished");
            ImGuiMCP::Spacing();
            ImGuiMCP::Separator();
            ImGuiMCP::Spacing();

            if (ImGuiMCP::Button("Yes, Convert", { 120, 0 })) {
                // Roda o processo
                RunConversionProcess();
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