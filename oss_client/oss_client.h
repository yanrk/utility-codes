#ifndef OSS_CLIENT_H
#define OSS_CLIENT_H


extern bool oss_upload_from_file(const std::string & oss_file, const std::string & path);
extern bool oss_upload_from_buffer(const std::string & oss_file, const std::string & content);


#endif // OSS_CLIENT_H
