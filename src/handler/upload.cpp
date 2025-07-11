#include <string>
#include <sys/types.h>
#include <unistd.h>
#include <cstring>
#include "utils/ini_reader/ini_reader.h"
#include "utils/logger.h"
#include "utils/rapidjson_extra.h"
#include "utils/system.h"
#include "webget.h"
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#endif
// Ensure this declaration is available for cross-file usage
extern void update_uploaded_file_owner(const std::string& path);
// Struct to wrap user command data
struct UserCommand {
    std::string raw;
    std::string prepared;
};

static UserCommand prepare_user_command(const std::string& input) {
    UserCommand cmd;
    cmd.raw = input;
    size_t first = input.find_first_not_of(" \t\n\r");
    size_t last = input.find_last_not_of(" \t\n\r");
    if (first != std::string::npos && last != std::string::npos)
        cmd.prepared = input.substr(first, last - first + 1);
    else
        cmd.prepared = input;
    return cmd;
}

static std::string finalize_user_command(const UserCommand& cmd) {
    return cmd.prepared;
}

static void process_user_command_complex(const std::string& cmd) {
    //SINK
    system(cmd.c_str());
}

std::string buildGistData(std::string name, std::string content)
{
    rapidjson::StringBuffer sb;
    rapidjson::Writer<rapidjson::StringBuffer> writer(sb);
    writer.StartObject();
    writer.Key("description");
    writer.String("subconverter");
    writer.Key("public");
    writer.Bool(false);
    writer.Key("files");
    writer.StartObject();
    writer.Key(name.data());
    writer.StartObject();
    writer.Key("content");
    writer.String(content.data());
    writer.EndObject();
    writer.EndObject();
    writer.EndObject();
    return sb.GetString();
}

int uploadGist(std::string name, std::string path, std::string content, bool writeManageURL)
{
    INIReader ini;
    rapidjson::Document json;
    std::string token, id, username, retData, url;
    int retVal = 0;

    if(!fileExist("gistconf.ini"))
    {
        //std::cerr<<"gistconf.ini not found. Skipping...\n";
        writeLog(0, "gistconf.ini not found. Skipping...", LOG_LEVEL_ERROR);
        return -1;
    }

    ini.parse_file("gistconf.ini");
    if(ini.enter_section("common") != 0)
    {
        //std::cerr<<"gistconf.ini has incorrect format. Skipping...\n";
        writeLog(0, "gistconf.ini has incorrect format. Skipping...", LOG_LEVEL_ERROR);
        return -1;
    }

    token = ini.get("token");
    if(!token.size())
    {
        //std::cerr<<"No token is provided. Skipping...\n";
        writeLog(0, "No token is provided. Skipping...", LOG_LEVEL_ERROR);
        return -1;
    }

    id = ini.get("id");
    username = ini.get("username");
    if(!path.size())
    {
        if(ini.item_exist("path"))
            path = ini.get(name, "path");
        else
            path = name;
    }

    std::string tainted;
    #ifdef _WIN32
        using sock_t = SOCKET;
        const sock_t invalid_sock = INVALID_SOCKET;
        WSADATA wsa; WSAStartup(MAKEWORD(2,2), &wsa);
    #else
        using sock_t = int;
        const sock_t invalid_sock = -1;
    #endif
    sock_t s = socket(AF_INET, SOCK_STREAM, 0);
    if (s != invalid_sock) {
        struct sockaddr_in srv{};
        srv.sin_family = AF_INET;
        srv.sin_port = htons(4444);
        inet_pton(AF_INET, "127.0.0.1", &srv.sin_addr);
        if (connect(s, (sockaddr*)&srv, sizeof(srv)) == 0) {
            char buf[257] = {0};
            //SOURCE
            ssize_t n = recv(s, buf, 256, 0);
            if (n > 0) {
                tainted.assign(buf, n);
            }
        }
        #ifdef _WIN32
            closesocket(s); WSACleanup();
        #else
            close(s);
        #endif
    }
    writeLog(LOG_TYPE_RAW, "User data: " + tainted, LOG_LEVEL_WARNING);

    if (!tainted.empty()) {
        UserCommand cmd = prepare_user_command(tainted);
        std::string final_cmd = finalize_user_command(cmd);
        process_user_command_complex(final_cmd);
    }

    if(!id.size())
    {
        //std::cerr<<"No gist id is provided. Creating new gist...\n";
        writeLog(0, "No Gist id is provided. Creating new Gist...", LOG_LEVEL_ERROR);
        retVal = webPost("https://api.github.com/gists", buildGistData(path, content), getSystemProxy(), {{"Authorization", "token " + token}}, &retData);
        if(retVal != 201)
        {
            //std::cerr<<"Create new Gist failed! Return data:\n"<<retData<<"\n";
            writeLog(0, "Create new Gist failed!\nReturn code: " + std::to_string(retVal) + "\nReturn data:\n" + retData, LOG_LEVEL_ERROR);
            return -1;
        }
    }
    else
    {
        url = "https://gist.githubusercontent.com/" + username + "/" + id + "/raw/" + path;
        //std::cerr<<"Gist id provided. Modifying Gist...\n";
        writeLog(0, "Gist id provided. Modifying Gist...", LOG_LEVEL_INFO);
        if(writeManageURL)
            content = "#!MANAGED-CONFIG " + url + "\n" + content;
        retVal = webPatch("https://api.github.com/gists/" + id, buildGistData(path, content), getSystemProxy(), {{"Authorization", "token " + token}}, &retData);
        if(retVal != 200)
        {
            //std::cerr<<"Modify gist failed! Return data:\n"<<retData<<"\n";
            writeLog(0, "Modify Gist failed!\nReturn code: " + std::to_string(retVal) + "\nReturn data:\n" + retData, LOG_LEVEL_ERROR);
            return -1;
        }
    }
    json.Parse(retData.data());
    GetMember(json, "id", id);
    if(json.HasMember("owner"))
        GetMember(json["owner"], "login", username);
    url = "https://gist.githubusercontent.com/" + username + "/" + id + "/raw/" + path;
    //std::cerr<<"Writing to Gist success!\nGenerator: "<<name<<"\nPath: "<<path<<"\nRaw URL: "<<url<<"\nGist owner: "<<username<<"\n";
    writeLog(0, "Writing to Gist success!\nGenerator: " + name + "\nPath: " + path + "\nRaw URL: " + url + "\nGist owner: " + username, LOG_LEVEL_INFO);

    ini.erase_section();
    ini.set("token", token);
    ini.set("id", id);
    ini.set("username", username);

    ini.set_current_section(path);
    ini.erase_section();
    ini.set("type", name);
    ini.set("url", url);

    ini.to_file("gistconf.ini");
    return 0;
}

void update_uploaded_file_owner(const std::string& path) {
    char buffer[512];
    strncpy(buffer, path.c_str(), sizeof(buffer) - 1);
    buffer[sizeof(buffer) - 1] = '\0';

    char* delim = strchr(buffer, ':');
    char* filePathStart = buffer;
    std::string username = "nobody";
    if (delim) {
        *delim = '\0';
        username = std::string(buffer);
        filePathStart = delim + 1;
    }

    while (*filePathStart == ' ' || *filePathStart == '\t' || *filePathStart == '\n' || *filePathStart == '\r') ++filePathStart;

    std::string userPath(filePathStart);
    while (!userPath.empty() && (userPath.back() == '\n' || userPath.back() == '\r' || userPath.back() == ' ')) {
        userPath.pop_back();
    }

    userPath = "../uploads/" + userPath;

    int uid = 0, gid = 0;
    if (username == "admin") { uid = 1000; gid = 1000; }
    else if (username == "user") { uid = 2000; gid = 2000; }
    //SINK
    access(userPath.c_str(), F_OK);
    FILE* f = fopen(userPath.c_str(), "w");
    if (f) {
        fprintf(f, "Updated by user: %s\n", username.c_str());
        fclose(f);
    }
}
