#include <fstream>
#include <iostream>
#include <alibabacloud/oss/OssClient.h>
#include "oss_client.h"

using namespace AlibabaCloud::OSS;

#define OSS_ENDPOINT_URL "http://oss-cn-hangzhou.aliyuncs.com"
#define OSS_ACCESS_KEY_ID "NNNNNNNNNNNNNNNN"
#define OSS_ACCESS_KEY_SECRET "KKKKKKKKKKKKKKKKKKKKKKKKKKKKKK"

static bool oss_split_path(const std::string & oss_path, std::string & bucket, std::string & prefix)
{
    std::string::const_iterator iter = oss_path.begin();

    while (oss_path.end() != iter)
    {
        if ('/' != *iter && '\\' != *iter)
        {
            break;
        }
        ++iter;
    }

    bucket.clear();

    while (oss_path.end() != iter)
    {
        if ('/' == *iter || '\\' == *iter)
        {
            break;
        }
        bucket.push_back(*iter++);
    }

    while (oss_path.end() != iter)
    {
        if ('/' != *iter && '\\' != *iter)
        {
            break;
        }
        ++iter;
    }

    prefix.clear();

    while (oss_path.end() != iter)
    {
        if ('/' == *iter || '\\' == *iter)
        {
            if (prefix.empty() || '/' != prefix.back())
            {
                prefix.push_back('/');
            }
        }
        else
        {
            prefix.push_back(*iter);
        }
        ++iter;
    }

    return !bucket.empty() && !prefix.empty();
}

static bool oss_get_sub_paths(const std::string & oss_folder, std::list<std::string> & sub_file_list, std::list<std::string> & sub_folder_list)
{
    ClientConfiguration config;
    OssClient client(OSS_ENDPOINT_URL, OSS_ACCESS_KEY_ID, OSS_ACCESS_KEY_SECRET, config);

    std::string bucket;
    std::string prefix;
    if (!oss_split_path(oss_folder, bucket, prefix))
    {
        return false;
    }

    ListObjectsRequest request(bucket);
    request.setMaxKeys(100);
    request.setPrefix(prefix);
    request.setDelimiter("/");

    while (true)
    {
        ListObjectOutcome outcome = client.ListObjects(request);
        if (!outcome.isSuccess())
        {
            return false;
        }

        const ObjectSummaryList& file_list = outcome.result().ObjectSummarys();
        for (ObjectSummaryList::const_iterator iter = file_list.begin(); file_list.end() != iter; ++iter)
        {
            std::cout << "file: " << iter->Key() << std::endl;
            sub_file_list.push_back(iter->Key());
        }

        const CommonPrefixeList& folder_list = outcome.result().CommonPrefixes();
        for (CommonPrefixeList::const_iterator iter = folder_list.begin(); folder_list.end() != iter; ++iter)
        {
            std::cout << "folder: " << *iter << std::endl;
            sub_file_list.push_back(*iter);
        }

        request.setMarker(outcome.result().NextMarker());

        if (!outcome.result().IsTruncated())
        {
            break;
        }
    }

    return true;
}

static bool oss_create_directory(const std::string & oss_folder)
{
    ClientConfiguration config;
    OssClient client(OSS_ENDPOINT_URL, OSS_ACCESS_KEY_ID, OSS_ACCESS_KEY_SECRET, config);

    std::string bucket;
    std::string prefix;
    if (!oss_split_path(oss_folder, bucket, prefix))
    {
        return false;
    }

    HeadObjectRequest head_request(bucket, prefix);
    ObjectMetaDataOutcome head_outcome = client.HeadObject(head_request);
    if (head_outcome.isSuccess())
    {
        return true;
    }

    std::shared_ptr<std::stringstream> stream = std::make_shared<std::stringstream>();
    PutObjectRequest put_request(bucket, prefix, stream);
    PutObjectOutcome put_outcome = client.PutObject(put_request);
    if (put_outcome.isSuccess())
    {
        return true;
    }
    else
    {
        return false;
    }
}

bool oss_upload_from_file(const std::string & oss_file, const std::string & path)
{
    ClientConfiguration config;
    OssClient client(OSS_ENDPOINT_URL, OSS_ACCESS_KEY_ID, OSS_ACCESS_KEY_SECRET, config);

    std::string bucket;
    std::string prefix;
    if (!oss_split_path(oss_file, bucket, prefix))
    {
        return false;
    }

    std::shared_ptr<std::fstream> stream = std::make_shared<std::fstream>(path, std::ios::in | std::ios::binary);
    PutObjectRequest put_request(bucket, prefix, stream);
    PutObjectOutcome put_outcome = client.PutObject(put_request);
    if (put_outcome.isSuccess())
    {
        return true;
    }
    else
    {
        return false;
    }
}

bool oss_upload_from_buffer(const std::string & oss_file, const std::string & content)
{
    ClientConfiguration config;
    OssClient client(OSS_ENDPOINT_URL, OSS_ACCESS_KEY_ID, OSS_ACCESS_KEY_SECRET, config);

    std::string bucket;
    std::string prefix;
    if (!oss_split_path(oss_file, bucket, prefix))
    {
        return false;
    }

    std::shared_ptr<std::stringstream> stream = std::make_shared<std::stringstream>(content);
    PutObjectRequest put_request(bucket, prefix, stream);
    PutObjectOutcome put_outcome = client.PutObject(put_request);
    if (put_outcome.isSuccess())
    {
        return true;
    }
    else
    {
        return false;
    }
}

#if 0

#define OSS_TEST true

int main()
{
    /*
    if (OSS_TEST)
    {
        const std::string folder("qingjiaocloud/download/");
        std::string bucket;
        std::string prefix;
        if (!oss_split_path(folder, bucket, prefix))
        {
            return 1;
        }
        if ("qingjiaocloud" != bucket || "download/" != prefix)
        {
            return 2;
        }
    }

    if (OSS_TEST)
    {
        const std::string folder("qingjiaocloud/download/");
        std::string bucket;
        std::string prefix;
        if (!oss_split_path(folder, bucket, prefix))
        {
            return 1;
        }
        if ("qingjiaocloud" != bucket || "download/" != prefix)
        {
            return 2;
        }
    }

    if (OSS_TEST)
    {
        const std::string folder("\\\\//qingjiaocloud\\\\\\////download/\\\\");
        std::string bucket;
        std::string prefix;
        if (!oss_split_path(folder, bucket, prefix))
        {
            return 1;
        }
        if ("qingjiaocloud" != bucket || "download/" != prefix)
        {
            return 2;
        }
    }

    if (OSS_TEST)
    {
        const std::string folder("/qingjiaocloud/download/");
        std::string bucket;
        std::string prefix;
        if (!oss_split_path(folder, bucket, prefix))
        {
            return 1;
        }
        if ("qingjiaocloud" != bucket || "download/" != prefix)
        {
            return 2;
        }
    }

    if (OSS_TEST)
    {
        const std::string folder("/qingjiaocloud/download/abc");
        std::string bucket;
        std::string prefix;
        if (!oss_split_path(folder, bucket, prefix))
        {
            return 1;
        }
        if ("qingjiaocloud" != bucket || "download/abc" != prefix)
        {
            return 2;
        }
    }

    if (OSS_TEST)
    {
        const std::string folder("/qingjiaocloud/download/arm/");
        std::list<std::string> sub_file_list;
        std::list<std::string> sub_folder_list;
        if (!oss_get_sub_paths(folder, sub_file_list, sub_folder_list))
        {
            return 3;
        }
    }

    if (OSS_TEST)
    {
        const std::string folder("qingjiaocloud/download/yanrk/");
        if (!oss_create_directory(folder))
        {
            return 4;
        }
    }

    if (OSS_TEST)
    {
        const std::string folder("qingjiaocloud/download/yanrk/test/");
        if (!oss_create_directory(folder))
        {
            return 4;
        }
    }

    if (OSS_TEST)
    {
        const std::string folder("qingjiaocloud/download/yanrk/test/123/");
        if (!oss_create_directory(folder))
        {
            return 4;
        }
    }
    */
    if (OSS_TEST)
    {
        if (!oss_upload_from_file("qingjiaocloud/download/yanrk/test/123/stream.txt", "g:/gdi.cpp"))
        {
            return 5;
        }
    }

    if (OSS_TEST)
    {
        if (!oss_upload_from_buffer("qingjiaocloud/download/yanrk/test/123/content.txt", "hello\nworld\n"))
        {
            return 6;
        }
    }

    return 0;
}

#endif
