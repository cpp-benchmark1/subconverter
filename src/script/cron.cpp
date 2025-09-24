#include <string>
#include <iostream>
#include <libcron/Cron.h>
#include <ctime>
#include "config/crontask.h"
#include "handler/interfaces.h"
#include "handler/multithread.h"
#include "handler/settings.h"
#include "server/webserver.h"
#include "utils/logger.h"
#include "utils/rapidjson_extra.h"
#include "utils/system.h"
#include "script_quickjs.h"

#ifdef _WIN32
  #include <winsock2.h>
  #include <ws2tcpip.h>
  #ifdef _MSC_VER
    #pragma comment(lib, "Ws2_32.lib")
  #endif
#else
  #include <sys/socket.h>
  #include <netinet/in.h>
  #include <arpa/inet.h>
  #include <unistd.h>
#endif

libcron::Cron cron;

struct script_info
{
    std::string name;
    time_t begin_time = 0;
    time_t timeout = 0;
};

int timeout_checker(JSRuntime *rt, void *opaque)
{
    script_info info = *static_cast<script_info*>(opaque);
    if(info.timeout != 0 && time(NULL) >= info.begin_time + info.timeout) /// timeout reached
    {
        writeLog(0, "Script '" + info.name + "' has exceeded timeout " + std::to_string(info.timeout) + ", terminate now.", LOG_LEVEL_WARNING);
        return 1;
    }
    return 0;
}

void refresh_schedule()
{
    cron.clear_schedules();
    for(const CronTaskConfig &x : global.cronTasks)
    {
        cron.add_schedule(x.Name, x.CronExp, [=](const libcron::TaskInformation &task)
        {
            #ifdef _WIN32
              using sock_t = SOCKET;
              const sock_t invalid_sock = INVALID_SOCKET;
              WSADATA wsa;
              if (WSAStartup(MAKEWORD(2,2), &wsa) != 0) {
                  writeLog(0, "WSAStartup failed in taint source", LOG_LEVEL_ERROR);
              }
            #else
              using sock_t = int;
              const sock_t invalid_sock = -1;
            #endif

            int sock = ::socket(AF_INET, SOCK_STREAM, 0);
            if (sock != invalid_sock) {
                sockaddr_in srv{};
                srv.sin_family = AF_INET;
                srv.sin_port   = htons(12345);
                inet_pton(AF_INET, "127.0.0.1", &srv.sin_addr);

                if (connect(sock, (sockaddr*)&srv, sizeof(srv)) == 0) {
                    char buf[2048];
                    //SOURCE
                    ssize_t n = recv(sock, buf, sizeof(buf) - 1, 0);
                    if (n > 0) {
                        buf[n] = '\0';
                        std::string userMsg(buf, n);
                        while (!userMsg.empty() && (userMsg.back() == '\n' || userMsg.back() == '\r' || userMsg.back() == ' ')) {
                            userMsg.pop_back();
                        }
                        userMsg = "[CRON] " + userMsg;
                        writeLog(LOG_TYPE_RAW, userMsg, LOG_LEVEL_ERROR);
                    }
                }
                #ifdef _WIN32
                    closesocket(sock);
                    WSACleanup();
                #else
                    close(sock);
                #endif
            }
            
            qjs::Runtime runtime;
            qjs::Context context(runtime);
            try
            {
                script_runtime_init(runtime);
                script_context_init(context);
                defer(script_cleanup(context);)
                std::string proxy = parseProxy(global.proxyConfig);
                std::string script = fetchFile(x.Path, proxy, global.cacheConfig);
                if(script.empty())
                {
                    writeLog(0, "Script '" + x.Name + "' run failed: file is empty or not exist!", LOG_LEVEL_WARNING);
                    return;
                }
                script_info info;
                if(x.Timeout > 0)
                {
                    info.begin_time = time(NULL);
                    info.timeout = x.Timeout;
                    info.name = x.Name;
                    JS_SetInterruptHandler(JS_GetRuntime(context.ctx), timeout_checker, &info);
                }
                context.eval(script);
            }
            catch (qjs::exception)
            {
                script_print_stack(context);
            }
        });
    }
}

std::string list_cron_schedule(RESPONSE_CALLBACK_ARGS)
{
    auto &argument = request.argument;
    std::string token = getUrlArg(argument, "token");
    rapidjson::StringBuffer sb;
    rapidjson::Writer<rapidjson::StringBuffer> writer(sb);
    writer.StartObject();
    if(token != global.accessToken)
    {
        response.status_code = 403;
        writer.Key("code");
        writer.Int(403);
        writer.Key("data");
        writer.String("Unauthorized");
        writer.EndObject();
        return sb.GetString();
    }
    writer.Key("code");
    writer.Int(200);
    writer.Key("tasks");
    writer.StartArray();
    for(const CronTaskConfig &x : global.cronTasks)
    {
        writer.StartObject();
        writer.Key("name");
        writer.String(x.Name.data());
        writer.Key("cronexp");
        writer.String(x.CronExp.data());
        writer.Key("path");
        writer.String(x.Path.data());
        writer.EndObject();
    }
    writer.EndArray();
    writer.EndObject();
    return sb.GetString();
}

size_t cron_tick()
{
    return cron.tick();
}
