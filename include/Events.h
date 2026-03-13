namespace Sink {
    // Variaveis de estado globais de input (para nao precisar mexer no seu Events.h)
    static bool ls_pressed = false; // Left Shift (0x2A)
    static bool q_pressed = false;  // Q (0x10)
    static bool e_pressed = false;  // E (0x12)
    static bool la_pressed = false; // Left Alt (0x38)
    static bool z_pressed = false;  // Z (0x2C)
    static bool x_pressed = false;  // X (0x2D)

    // Variaveis do Right Thumbstick
    static bool rs_up = false;
    static bool rs_down = false;
    static bool rs_left = false;
    static bool rs_right = false;

    inline uint32_t keyForward = 0x11;
    inline uint32_t keyBack = 0x1F;
    inline uint32_t keyLeft = 0x1E;
    inline uint32_t keyRight = 0x20;

    class InputListener : public RE::BSTEventSink<RE::InputEvent*> {
    public:
        // Singleton para garantir que exista apenas uma instância
        static InputListener* GetSingleton() {
            static InputListener singleton;
            return &singleton;
        }

        static inline RE::INPUT_DEVICE lastUsedDevice = RE::INPUT_DEVICE::kKeyboard;
        // A função que processa os eventos de input do jogo
        virtual RE::BSEventNotifyControl ProcessEvent(RE::InputEvent* const* a_event,
            RE::BSTEventSource<RE::InputEvent*>* a_eventSource) override;
        static int GetDirectionalState() { return directionalState; };

    protected:

    private:
        // Função para calcular a direção com base nas teclas pressionadas
        void UpdateDirectionalState();
        static inline int directionalState = 0;
        // Variáveis para rastrear o estado de cada tecla de movimento
        bool w_pressed = false;
        bool a_pressed = false;
        bool s_pressed = false;
        bool d_pressed = false;

        // Controle
        bool c_up = false;
        bool c_left = false;
        bool c_down = false;
        bool c_right = false;
    };
    class MenuWatcher : public RE::BSTEventSink<RE::MenuOpenCloseEvent>
    {
    public:
        static MenuWatcher* GetSingleton()
        {
            static MenuWatcher singleton;
            return &singleton;
        }

        void Register()
        {
            auto ui = RE::UI::GetSingleton();
            if (ui) {
                ui->AddEventSink(this);
                SKSE::log::info("MenuWatcher registrado para monitorar o JournalMenu.");
            }
        }

        RE::BSEventNotifyControl ProcessEvent(const RE::MenuOpenCloseEvent* a_event, RE::BSTEventSource<RE::MenuOpenCloseEvent>*) override;
    };

    void UpdateRegisteredHotkeys();
}