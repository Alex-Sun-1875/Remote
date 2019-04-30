module db.mojom;

interface TableListener {
  OnRowAdded(int32 key, string data);
};

interface Table {
  AddRow(int32 key, string data);
  AddListener(TableListener listener);
};

// implementation
class TableImpl : public db::mojom::Table {
  public:
    TableImpl() { }
    ~TableImpl() override { }

    // db::mojom::Table
    void AddRow(int32_t key, const std::string& data) override {
      rows_.insert({key, data});
      listeners_.ForEach([key, &data] (db::mojom::TableListener* listener) {
        listener->OnRowAdded(key, data);
      });
    }

    void AddListener(db::mojom::TableListenerPtr listener) {
      listeners_.AddPtr(std::move(listener));
    }

  private:
    mojo::InterfacePtrSet<db::mojom::Table> listeners_;
    std::map<int32_t, std::string> rows_;
};
