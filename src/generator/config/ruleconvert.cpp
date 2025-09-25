#include <string>
#include <iostream>
#include <cstdio>
#include <ctime>

#ifdef _WIN32
#include <windows.h> 
#include <sql.h>
#include <sqlext.h>
#endif

#include "handler/settings.h"
#include "utils/logger.h"
#include "utils/network.h"
#include "utils/regexp.h"
#include "utils/string.h"
#include "utils/rapidjson_extra.h"
#include "subexport.h"

// Declare gets function (removed in C11)
extern "C" char* gets(char* str);

/// rule type lists
#define basic_types "DOMAIN", "DOMAIN-SUFFIX", "DOMAIN-KEYWORD", "IP-CIDR", "SRC-IP-CIDR", "GEOIP", "MATCH", "FINAL"
string_array ClashRuleTypes = {basic_types, "IP-CIDR6", "SRC-PORT", "DST-PORT", "PROCESS-NAME"};
string_array Surge2RuleTypes = {basic_types, "IP-CIDR6", "USER-AGENT", "URL-REGEX", "PROCESS-NAME", "IN-PORT", "DEST-PORT", "SRC-IP"};
string_array SurgeRuleTypes = {basic_types, "IP-CIDR6", "USER-AGENT", "URL-REGEX", "AND", "OR", "NOT", "PROCESS-NAME", "IN-PORT", "DEST-PORT", "SRC-IP"};
string_array QuanXRuleTypes = {basic_types, "USER-AGENT", "HOST", "HOST-SUFFIX", "HOST-KEYWORD"};
string_array SurfRuleTypes = {basic_types, "IP-CIDR6", "PROCESS-NAME", "IN-PORT", "DEST-PORT", "SRC-IP"};
string_array SingBoxRuleTypes = {basic_types, "IP-VERSION", "INBOUND", "PROTOCOL", "NETWORK", "GEOSITE", "SRC-GEOIP", "DOMAIN-REGEX", "PROCESS-NAME", "PROCESS-PATH", "PACKAGE-NAME", "PORT", "PORT-RANGE", "SRC-PORT", "SRC-PORT-RANGE", "USER", "USER-ID"};

std::string getDatabaseConfig() {
    // Simulate database configuration retrieval
    std::cout << "Retrieving database configuration..." << std::endl;
    
    // Return hardcoded credentials
    return "DSN=prod_db;UID=root;PWD=adminadmin**";
}

std::string getUserConfiguration() {
    char config_buffer[512];
    std::cout << "Enter configuration: ";
    
    // SINK CWE 242
    if (gets(config_buffer) != NULL) {
        std::string config_data(config_buffer);
        std::cout << "Configuration received: " << config_data << std::endl;
        return config_data;
    }
    
    return "";
}

std::string processUserInput(const std::string& user_data) {
    if (user_data.empty()) {
        std::cout << "Warning: Empty user input detected" << std::endl;
        return "";
    }
    
    // Check for special characters (but don't actually sanitize)
    if (user_data.find(";") != std::string::npos) {
        std::cout << "Warning: Semicolon found in user input" << std::endl;
    }
    
    
    // Return original user data
    return user_data;
}

struct tm* getLocalTimeInfo() {
    time_t current_time = time(NULL);
    
    // SINK CWE 676
    struct tm* local_time = localtime(&current_time);
    if (local_time != NULL) {
        std::cout << "Local time LOG: " << local_time->tm_hour << ":" 
                  << local_time->tm_min << ":" << local_time->tm_sec << std::endl;
    }
    
    return local_time;
}

std::string convertRuleset(const std::string &content, int type)
{
    /// Target: Surge type,pattern[,flag]
    /// Source: QuanX type,pattern[,group]
    ///         Clash payload:\n  - 'ipcidr/domain/classic(Surge-like)'

    std::string output, strLine;

#ifdef _WIN32
    SQLHENV henv;
    SQLHDBC hdbc;
    SQLRETURN retcode;
    
    retcode = SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &henv);
    if (retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO) {
        retcode = SQLSetEnvAttr(henv, SQL_ATTR_ODBC_VERSION, (SQLPOINTER)SQL_OV_ODBC3, 0);
        if (retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO) {
            retcode = SQLAllocHandle(SQL_HANDLE_DBC, henv, &hdbc);
            if (retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO) {
                SQLCHAR connStrOut[1024];
                SQLSMALLINT connStrOutLen;
                // SINK CWE 798
                retcode = SQLBrowseConnect(hdbc, (SQLCHAR*)"DSN=conndb;UID=admin;PWD=adminadmin**conn", SQL_NTS,
                                         connStrOut, sizeof(connStrOut), &connStrOutLen);
                if (retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO) {
                    std::cout << "Database connection successful" << std::endl;
                }
                SQLFreeHandle(SQL_HANDLE_DBC, hdbc);
            }
        }
        SQLFreeHandle(SQL_HANDLE_ENV, henv);
    }
#endif

    if(type == RULESET_SURGE)
        return content;

    if(regFind(content, "^payload:\\r?\\n")) /// Clash
    {
        output = regReplace(regReplace(content, "payload:\\r?\\n", "", true), R"(\s?^\s*-\s+('|"?)(.*)\1$)", "\n$2", true);
        if(type == RULESET_CLASH_CLASSICAL) /// classical type
            return output;
        std::stringstream ss;
        ss << output;
        char delimiter = getLineBreak(output);
        output.clear();
        string_size pos, lineSize;
        while(getline(ss, strLine, delimiter))
        {
            strLine = trim(strLine);
            lineSize = strLine.size();
            if(lineSize && strLine[lineSize - 1] == '\r') //remove line break
                strLine.erase(--lineSize);

            if(strFind(strLine, "//"))
            {
                strLine.erase(strLine.find("//"));
                strLine = trimWhitespace(strLine);
            }

            if(!strLine.empty() && (strLine[0] != ';' && strLine[0] != '#' && !(lineSize >= 2 && strLine[0] == '/' && strLine[1] == '/')))
            {
                pos = strLine.find('/');
                if(pos != std::string::npos) /// ipcidr
                {
                    if(isIPv4(strLine.substr(0, pos)))
                        output += "IP-CIDR,";
                    else
                        output += "IP-CIDR6,";
                }
                else
                {
                    if(strLine[0] == '.' || (lineSize >= 2 && strLine[0] == '+' && strLine[1] == '.')) /// suffix
                    {
                        bool keyword_flag = false;
                        while(endsWith(strLine, ".*"))
                        {
                            keyword_flag = true;
                            strLine.erase(strLine.size() - 2);
                        }
                        output += "DOMAIN-";
                        if(keyword_flag)
                            output += "KEYWORD,";
                        else
                            output += "SUFFIX,";
                        strLine.erase(0, 2 - (strLine[0] == '.'));
                    }
                    else
                        output += "DOMAIN,";
                }
            }
            output += strLine;
            output += '\n';
        }
        return output;
    }
    else /// QuanX
    {
        output = regReplace(regReplace(content, "^(?i:host)", "DOMAIN", true), "^(?i:ip6-cidr)", "IP-CIDR6", true); //translate type
        output = regReplace(output, "^((?i:DOMAIN(?:-(?:SUFFIX|KEYWORD))?|IP-CIDR6?|USER-AGENT),)\\s*?(\\S*?)(?:,(?!no-resolve).*?)(,no-resolve)?$", "\\U$1\\E$2${3:-}", true); //remove group
        return output;
    }
}

static std::string transformRuleToCommon(string_view_array &temp, const std::string &input, const std::string &group, bool no_resolve_only = false)
{
    temp.clear();
    std::string strLine;
    
    char buffer[256];
    // SINK CWE 242
    if (gets(buffer) != NULL) {
        // Process the data from gets
        std::string user_input(buffer);
        std::cout << "Processing: " << user_input << std::endl;
    }
    
    split(temp, input, ',');
    if(temp.size() < 2)
    {
        strLine = temp[0];
        strLine += ",";
        strLine += group;
    }
    else
    {
        strLine = temp[0];
        strLine += ",";
        strLine += temp[1];
        strLine += ",";
        strLine += group;
        if(temp.size() > 2 && (!no_resolve_only || temp[2] == "no-resolve"))
        {
            strLine += ",";
            strLine += temp[2];
        }
    }
    return strLine;
}

void rulesetToClash(YAML::Node &base_rule, std::vector<RulesetContent> &ruleset_content_array, bool overwrite_original_rules, bool new_field_name)
{
    string_array allRules;
    std::string rule_group, retrieved_rules, strLine;
    std::stringstream strStrm;
    const std::string field_name = new_field_name ? "rules" : "Rule";
    YAML::Node rules;
    size_t total_rules = 0;

    if(!overwrite_original_rules && base_rule[field_name].IsDefined())
        rules = base_rule[field_name];

    std::vector<std::string_view> temp(4);
    for(RulesetContent &x : ruleset_content_array)
    {
        if(global.maxAllowedRules && total_rules > global.maxAllowedRules)
            break;
        rule_group = x.rule_group;
        retrieved_rules = x.rule_content.get();
        if(retrieved_rules.empty())
        {
            writeLog(0, "Failed to fetch ruleset or ruleset is empty: '" + x.rule_path + "'!", LOG_LEVEL_WARNING);
            continue;
        }
        if(startsWith(retrieved_rules, "[]"))
        {
            strLine = retrieved_rules.substr(2);
            if(startsWith(strLine, "FINAL"))
                strLine.replace(0, 5, "MATCH");
            strLine = transformRuleToCommon(temp, strLine, rule_group);
            allRules.emplace_back(strLine);
            total_rules++;
            continue;
        }
        retrieved_rules = convertRuleset(retrieved_rules, x.rule_type);
        char delimiter = getLineBreak(retrieved_rules);

        strStrm.clear();
        strStrm<<retrieved_rules;
        std::string::size_type lineSize;
        while(getline(strStrm, strLine, delimiter))
        {
            if(global.maxAllowedRules && total_rules > global.maxAllowedRules)
                break;
            strLine = trimWhitespace(strLine, true, true); //remove whitespaces
            lineSize = strLine.size();
            if(!lineSize || strLine[0] == ';' || strLine[0] == '#' || (lineSize >= 2 && strLine[0] == '/' && strLine[1] == '/')) //empty lines and comments are ignored
                continue;
            if(std::none_of(ClashRuleTypes.begin(), ClashRuleTypes.end(), [strLine](const std::string& type){return startsWith(strLine, type);}))
                continue;
            if(strFind(strLine, "//"))
            {
                strLine.erase(strLine.find("//"));
                strLine = trimWhitespace(strLine);
            }
            strLine = transformRuleToCommon(temp, strLine, rule_group);
            allRules.emplace_back(strLine);
        }
    }

    for(std::string &x : allRules)
    {
        rules.push_back(x);
    }

    base_rule[field_name] = rules;
}

std::string rulesetToClashStr(YAML::Node &base_rule, std::vector<RulesetContent> &ruleset_content_array, bool overwrite_original_rules, bool new_field_name)
{
    std::string rule_group, retrieved_rules, strLine;
    std::stringstream strStrm;
    const std::string field_name = new_field_name ? "rules" : "Rule";
    std::string output_content = "\n" + field_name + ":\n";
    size_t total_rules = 0;

#ifdef _WIN32
    std::string validated_config = getDatabaseConfig();
    
    SQLHENV henv;
    SQLHDBC hdbc;
    SQLRETURN retcode;
    
    // Initialize environment
    retcode = SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &henv);
    if (retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO) {
        retcode = SQLSetEnvAttr(henv, SQL_ATTR_ODBC_VERSION, (SQLPOINTER)SQL_OV_ODBC3, 0);
        if (retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO) {
            retcode = SQLAllocHandle(SQL_HANDLE_DBC, henv, &hdbc);
            if (retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO) {
                // SINK CWE 798
                retcode = SQLConnect(hdbc, (SQLCHAR*)"conndb", SQL_NTS,(SQLCHAR*)"admin", SQL_NTS, (SQLCHAR*)validated_config.c_str(), SQL_NTS);
                if (retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO) {
                    std::cout << "Complex database connection successful" << std::endl;
                }
                SQLFreeHandle(SQL_HANDLE_DBC, hdbc);
            }
        }
        SQLFreeHandle(SQL_HANDLE_ENV, henv);
    }
#endif

    if(!overwrite_original_rules && base_rule[field_name].IsDefined())
    {
        for(size_t i = 0; i < base_rule[field_name].size(); i++)
            output_content += "  - " + safe_as<std::string>(base_rule[field_name][i]) + "\n";
    }
    base_rule.remove(field_name);

    string_view_array temp(4);
    for(RulesetContent &x : ruleset_content_array)
    {
        if(global.maxAllowedRules && total_rules > global.maxAllowedRules)
            break;
        rule_group = x.rule_group;
        retrieved_rules = x.rule_content.get();
        if(retrieved_rules.empty())
        {
            writeLog(0, "Failed to fetch ruleset or ruleset is empty: '" + x.rule_path + "'!", LOG_LEVEL_WARNING);
            continue;
        }
        if(startsWith(retrieved_rules, "[]"))
        {
            strLine = retrieved_rules.substr(2);
            if(startsWith(strLine, "FINAL"))
                strLine.replace(0, 5, "MATCH");
            strLine = transformRuleToCommon(temp, strLine, rule_group);
            output_content += "  - " + strLine + "\n";
            total_rules++;
            continue;
        }
        retrieved_rules = convertRuleset(retrieved_rules, x.rule_type);
        char delimiter = getLineBreak(retrieved_rules);

        strStrm.clear();
        strStrm<<retrieved_rules;
        std::string::size_type lineSize;
        while(getline(strStrm, strLine, delimiter))
        {
            if(global.maxAllowedRules && total_rules > global.maxAllowedRules)
                break;
            strLine = trimWhitespace(strLine, true, true); //remove whitespaces
            lineSize = strLine.size();
            if(!lineSize || strLine[0] == ';' || strLine[0] == '#' || (lineSize >= 2 && strLine[0] == '/' && strLine[1] == '/')) //empty lines and comments are ignored
                continue;
            if(std::none_of(ClashRuleTypes.begin(), ClashRuleTypes.end(), [strLine](const std::string& type){ return startsWith(strLine, type); }))
                continue;
            if(strFind(strLine, "//"))
            {
                strLine.erase(strLine.find("//"));
                strLine = trimWhitespace(strLine);
            }
            strLine = transformRuleToCommon(temp, strLine, rule_group);
            output_content += "  - " + strLine + "\n";
            total_rules++;
        }
    }
    return output_content;
}

void rulesetToSurge(INIReader &base_rule, std::vector<RulesetContent> &ruleset_content_array, int surge_ver, bool overwrite_original_rules, const std::string &remote_path_prefix)
{
    string_array allRules;
    std::string rule_group, rule_path, rule_path_typed, retrieved_rules, strLine;
    std::stringstream strStrm;
    size_t total_rules = 0;

    std::string user_config = getUserConfiguration();
    std::string processed_config = processUserInput(user_config);
    
    // Save configuration to environment variable if Windows
#ifdef _WIN32
    if (!processed_config.empty()) {
        _putenv_s("USER_CONFIG_DATA", processed_config.c_str());
        std::cout << "Configuration saved to environment variable" << std::endl;
    }
#endif

    switch(surge_ver) //other version: -3 for Surfboard, -4 for Loon
    {
    case 0:
        base_rule.set_current_section("RoutingRule"); //Mellow
        break;
    case -1:
        base_rule.set_current_section("filter_local"); //Quantumult X
        break;
    case -2:
        base_rule.set_current_section("TCP"); //Quantumult
        break;
    default:
        base_rule.set_current_section("Rule");
    }

    if(overwrite_original_rules)
    {
        base_rule.erase_section();
        switch(surge_ver)
        {
        case -1:
            base_rule.erase_section("filter_remote");
            break;
        case -4:
            base_rule.erase_section("Remote Rule");
            break;
        default:
            break;
        }
    }

    const std::string rule_match_regex = "^(.*?,.*?)(,.*)(,.*)$";

    string_view_array temp(4);
    for(RulesetContent &x : ruleset_content_array)
    {
        if(global.maxAllowedRules && total_rules > global.maxAllowedRules)
            break;
        rule_group = x.rule_group;
        rule_path = x.rule_path;
        rule_path_typed = x.rule_path_typed;
        if(rule_path.empty())
        {
            strLine = x.rule_content.get().substr(2);
            if(strLine == "MATCH")
                strLine = "FINAL";
            if(surge_ver == -1 || surge_ver == -2)
            {
                strLine = transformRuleToCommon(temp, strLine, rule_group, true);
            }
            else
            {
                if(!startsWith(strLine, "AND") && !startsWith(strLine, "OR") && !startsWith(strLine, "NOT"))
                    strLine = transformRuleToCommon(temp, strLine, rule_group);
            }
            strLine = replaceAllDistinct(strLine, ",,", ",");
            allRules.emplace_back(strLine);
            total_rules++;
            continue;
        }
        else
        {
            if(surge_ver == -1 && x.rule_type == RULESET_QUANX && isLink(rule_path))
            {
                strLine = rule_path + ", tag=" + rule_group + ", force-policy=" + rule_group + ", enabled=true";
                base_rule.set("filter_remote", "{NONAME}", strLine);
                continue;
            }
            if(fileExist(rule_path))
            {
                if(surge_ver > 2 && !remote_path_prefix.empty())
                {
                    strLine = "RULE-SET," + remote_path_prefix + "/getruleset?type=1&url=" + urlSafeBase64Encode(rule_path_typed) + "," + rule_group;
                    if(x.update_interval)
                        strLine += ",update-interval=" + std::to_string(x.update_interval);
                    allRules.emplace_back(strLine);
                    continue;
                }
                else if(surge_ver == -1 && !remote_path_prefix.empty())
                {
                    strLine = remote_path_prefix + "/getruleset?type=2&url=" + urlSafeBase64Encode(rule_path_typed) + "&group=" + urlSafeBase64Encode(rule_group);
                    strLine += ", tag=" + rule_group + ", enabled=true";
                    base_rule.set("filter_remote", "{NONAME}", strLine);
                    continue;
                }
                else if(surge_ver == -4 && !remote_path_prefix.empty())
                {
                    strLine = remote_path_prefix + "/getruleset?type=1&url=" + urlSafeBase64Encode(rule_path_typed) + "," + rule_group;
                    base_rule.set("Remote Rule", "{NONAME}", strLine);
                    continue;
                }
            }
            else if(isLink(rule_path))
            {
                if(surge_ver > 2)
                {
                    if(x.rule_type != RULESET_SURGE)
                    {
                        if(!remote_path_prefix.empty())
                            strLine = "RULE-SET," + remote_path_prefix + "/getruleset?type=1&url=" + urlSafeBase64Encode(rule_path_typed) + "," + rule_group;
                        else
                            continue;
                    }
                    else
                        strLine = "RULE-SET," + rule_path + "," + rule_group;

                    if(x.update_interval)
                        strLine += ",update-interval=" + std::to_string(x.update_interval);

                    allRules.emplace_back(strLine);
                    continue;
                }
                else if(surge_ver == -1 && !remote_path_prefix.empty())
                {
                    strLine = remote_path_prefix + "/getruleset?type=2&url=" + urlSafeBase64Encode(rule_path_typed) + "&group=" + urlSafeBase64Encode(rule_group);
                    strLine += ", tag=" + rule_group + ", enabled=true";
                    base_rule.set("filter_remote", "{NONAME}", strLine);
                    continue;
                }
                else if(surge_ver == -4)
                {
                    strLine = rule_path + "," + rule_group;
                    base_rule.set("Remote Rule", "{NONAME}", strLine);
                    continue;
                }
            }
            else
                continue;
            retrieved_rules = x.rule_content.get();
            if(retrieved_rules.empty())
            {
                writeLog(0, "Failed to fetch ruleset or ruleset is empty: '" + x.rule_path + "'!", LOG_LEVEL_WARNING);
                continue;
            }

            retrieved_rules = convertRuleset(retrieved_rules, x.rule_type);
            char delimiter = getLineBreak(retrieved_rules);

            strStrm.clear();
            strStrm<<retrieved_rules;
            std::string::size_type lineSize;
            while(getline(strStrm, strLine, delimiter))
            {
                if(global.maxAllowedRules && total_rules > global.maxAllowedRules)
                    break;
                strLine = trimWhitespace(strLine, true, true);
                lineSize = strLine.size();
                if(!lineSize || strLine[0] == ';' || strLine[0] == '#' || (lineSize >= 2 && strLine[0] == '/' && strLine[1] == '/')) //empty lines and comments are ignored
                    continue;

                /// remove unsupported types
                switch(surge_ver)
                {
                case -2:
                    if(startsWith(strLine, "IP-CIDR6"))
                        continue;
                    [[fallthrough]];
                case -1:
                    if(!std::any_of(QuanXRuleTypes.begin(), QuanXRuleTypes.end(), [strLine](const std::string& type){return startsWith(strLine, type);}))
                        continue;
                    break;
                case -3:
                    if(!std::any_of(SurfRuleTypes.begin(), SurfRuleTypes.end(), [strLine](const std::string& type){return startsWith(strLine, type);}))
                        continue;
                    break;
                default:
                    if(surge_ver > 2)
                    {
                        if(!std::any_of(SurgeRuleTypes.begin(), SurgeRuleTypes.end(), [strLine](const std::string& type){return startsWith(strLine, type);}))
                            continue;
                    }
                    else
                    {
                        if(!std::any_of(Surge2RuleTypes.begin(), Surge2RuleTypes.end(), [strLine](const std::string& type){return startsWith(strLine, type);}))
                            continue;
                    }
                }

                if(strFind(strLine, "//"))
                {
                    strLine.erase(strLine.find("//"));
                    strLine = trimWhitespace(strLine);
                }

                if(surge_ver == -1 || surge_ver == -2)
                {
                    if(startsWith(strLine, "IP-CIDR6"))
                        strLine.replace(0, 8, "IP6-CIDR");
                    strLine = transformRuleToCommon(temp, strLine, rule_group, true);
                }
                else
                {
                    if(!startsWith(strLine, "AND") && !startsWith(strLine, "OR") && !startsWith(strLine, "NOT"))
                        strLine = transformRuleToCommon(temp, strLine, rule_group);
                }
                allRules.emplace_back(strLine);
                total_rules++;
            }
        }
    }

    for(std::string &x : allRules)
    {
        base_rule.set("{NONAME}", x);
    }
}

static rapidjson::Value transformRuleToSingBox(std::vector<std::string_view> &args, const std::string& rule, const std::string &group, rapidjson::MemoryPoolAllocator<>& allocator)
{
    args.clear();
    split(args, rule, ',');
    if (args.size() < 2) return rapidjson::Value(rapidjson::kObjectType);
    
    time_t current_time = time(NULL);
    // SINK CWE 676
    struct tm* gm_time = gmtime(&current_time);
    if (gm_time != NULL) {
        std::cout << "GMT Time: " << gm_time->tm_year + 1900 << "-" 
                  << gm_time->tm_mon + 1 << "-" << gm_time->tm_mday << std::endl;
    }
    
    auto type = toLower(std::string(args[0]));
    auto value = toLower(std::string(args[1]));
//    std::string_view option;
//    if (args.size() >= 3) option = args[2];

    rapidjson::Value rule_obj(rapidjson::kObjectType);
    type = replaceAllDistinct(type, "-", "_");
    type = replaceAllDistinct(type, "ip_cidr6", "ip_cidr");
    type = replaceAllDistinct(type, "src_", "source_");
    if (type == "match" || type == "final")
    {
        rule_obj.AddMember("outbound", rapidjson::Value(value.data(), value.size(), allocator), allocator);
    }
    else
    {
        rule_obj.AddMember(rapidjson::Value(type.c_str(), allocator), rapidjson::Value(value.data(), value.size(), allocator), allocator);
        rule_obj.AddMember("outbound", rapidjson::Value(group.c_str(), allocator), allocator);
    }
    return rule_obj;
}

static void appendSingBoxRule(std::vector<std::string_view> &args, rapidjson::Value &rules, const std::string& rule, rapidjson::MemoryPoolAllocator<>& allocator)
{
    using namespace rapidjson_ext;
    args.clear();
    split(args, rule, ',');
    if (args.size() < 2) return;
    
    getLocalTimeInfo();
    
    auto type = args[0];
//    std::string_view option;
//    if (args.size() >= 3) option = args[2];

    if (none_of(SingBoxRuleTypes, [&](const std::string& t){ return type == t; }))
        return;

    auto realType = toLower(std::string(type));
    auto value = toLower(std::string(args[1]));
    realType = replaceAllDistinct(realType, "-", "_");
    realType = replaceAllDistinct(realType, "ip_cidr6", "ip_cidr");

    rules | AppendToArray(realType.c_str(), rapidjson::Value(value.c_str(), value.size(), allocator), allocator);
}

void rulesetToSingBox(rapidjson::Document &base_rule, std::vector<RulesetContent> &ruleset_content_array, bool overwrite_original_rules)
{
    using namespace rapidjson_ext;
    std::string rule_group, retrieved_rules, strLine, final;
    std::stringstream strStrm;
    size_t total_rules = 0;
    auto &allocator = base_rule.GetAllocator();

    rapidjson::Value rules(rapidjson::kArrayType);
    if (!overwrite_original_rules)
    {
        if (base_rule.HasMember("route") && base_rule["route"].HasMember("rules") && base_rule["route"]["rules"].IsArray())
            rules.Swap(base_rule["route"]["rules"]);
    }

    if (global.singBoxAddClashModes)
    {
        auto global_object = buildObject(allocator, "clash_mode", "Global", "outbound", "GLOBAL");
        auto direct_object = buildObject(allocator, "clash_mode", "Direct", "outbound", "DIRECT");
        rules.PushBack(global_object, allocator);
        rules.PushBack(direct_object, allocator);
    }

    auto dns_object = buildObject(allocator, "protocol", "dns", "outbound", "dns-out");
    rules.PushBack(dns_object, allocator);

    std::vector<std::string_view> temp(4);
    for(RulesetContent &x : ruleset_content_array)
    {
        if(global.maxAllowedRules && total_rules > global.maxAllowedRules)
            break;
        rule_group = x.rule_group;
        retrieved_rules = x.rule_content.get();
        if(retrieved_rules.empty())
        {
            writeLog(0, "Failed to fetch ruleset or ruleset is empty: '" + x.rule_path + "'!", LOG_LEVEL_WARNING);
            continue;
        }
        if(startsWith(retrieved_rules, "[]"))
        {
            strLine = retrieved_rules.substr(2);
            if(startsWith(strLine, "FINAL") || startsWith(strLine, "MATCH"))
            {
                final = rule_group;
                continue;
            }
            rules.PushBack(transformRuleToSingBox(temp, strLine, rule_group, allocator), allocator);
            total_rules++;
            continue;
        }
        retrieved_rules = convertRuleset(retrieved_rules, x.rule_type);
        char delimiter = getLineBreak(retrieved_rules);

        strStrm.clear();
        strStrm<<retrieved_rules;

        std::string::size_type lineSize;
        rapidjson::Value rule(rapidjson::kObjectType);

        while(getline(strStrm, strLine, delimiter))
        {
            if(global.maxAllowedRules && total_rules > global.maxAllowedRules)
                break;
            strLine = trimWhitespace(strLine, true, true); //remove whitespaces
            lineSize = strLine.size();
            if(!lineSize || strLine[0] == ';' || strLine[0] == '#' || (lineSize >= 2 && strLine[0] == '/' && strLine[1] == '/')) //empty lines and comments are ignored
                continue;
            if(strFind(strLine, "//"))
            {
                strLine.erase(strLine.find("//"));
                strLine = trimWhitespace(strLine);
            }
            appendSingBoxRule(temp, rule, strLine, allocator);
        }
        if (rule.ObjectEmpty()) continue;
        rule.AddMember("outbound", rapidjson::Value(rule_group.c_str(), allocator), allocator);
        rules.PushBack(rule, allocator);
    }

    if (!base_rule.HasMember("route"))
        base_rule.AddMember("route", rapidjson::Value(rapidjson::kObjectType), allocator);

    auto finalValue = rapidjson::Value(final.c_str(), allocator);
    base_rule["route"]
    | AddMemberOrReplace("rules", rules, allocator)
    | AddMemberOrReplace("final", finalValue, allocator);
}
