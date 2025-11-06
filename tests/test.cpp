









#include "Application.hpp"
#include "WebServer.hpp"

poolpoolserver::ServerConfig config;
config.port = 8080;
config.thread_num = 4;

poolpoolserver::Application::Initialize(config);

// 2. 设置业务逻辑
auto* server = poolpoolserver::Application::GetWebServer();
server->SetRequestHandler([](auto request) {
    // 使用内存池创建响应
    auto* pool = poolpoolserver::Application::GetResponsePool();
    auto response = pool->Construct();
    
    // 使用连接池查询数据库
    auto db_pool = poolpoolserver::Application::GetDatabasePool();
    auto conn = db_pool->GetConnection();
    conn->Execute("SELECT ...");
    
    response->SetStatus(200);
    response->SetBody("Hello World");
    return response;
});

// 3. 启动服务器
poolpoolserver::Application::Run();
