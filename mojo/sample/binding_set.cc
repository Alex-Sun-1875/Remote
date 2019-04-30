moudle system.mojom

interface Logger {
  Log(string message);
};

interface LoggerProvider {
  GetLogger(Logger& logger);
};

// implementation

class LogManager : public system::mojom::LoggerProvider,
                      public systen::mojom::Logger {
  public:
    explicit LogManager(system::mojom::LoggerProviderRequest request)
        : provide_binding_(this, std::move(request)) { }
    ~LogManager() { }

    // system::mojom::LoggerProvider
    void GetLogger(LoggerRequest request) override {
      logger_bindings_.AddBinding(this, std::move(request));
    }

    // system::mojom::Logger
    void Log(const std::string& message) override {
      LOG(ERROR) << "[Logger] " << message;
    }

  private:
    mojo::Binding<system::mojom::LoggerProvider> provider_binding_;
    mojo::BindingSet<system::mojom::Logger> logger_bindings_;
}
