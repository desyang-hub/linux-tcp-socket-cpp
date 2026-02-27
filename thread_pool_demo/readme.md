ThreadPool {
private:
    workers;
    tasks;
    m_mutex;
    m_condition
    bool stop;

public:
    ThreadPool();
    ~ThreadPool();

    template<class FUNC>
    void submit(FUNC &&job);
}