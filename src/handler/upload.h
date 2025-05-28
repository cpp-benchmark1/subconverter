#ifndef UPLOAD_H_INCLUDED
#define UPLOAD_H_INCLUDED

#include <string>

std::string buildGistData(std::string name, std::string content);
int uploadGist(std::string name, std::string path, std::string content, bool writeManageURL);

void update_uploaded_file_owner(const std::string& path);
#endif // UPLOAD_H_INCLUDED
