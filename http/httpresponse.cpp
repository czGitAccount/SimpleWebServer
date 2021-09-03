<<<<<<< HEAD
=======
/*
 * @Author       : mark
 * @Date         : 2020-06-27
 * @copyleft Apache 2.0
 */ 

// 原作者：mark, 以下为个人学习后进行的复现，增加注释，并进行了部分的修改

>>>>>>> def492361972feedee60578307251dcb6ce473b1
#include "httpresponse.h"

using namespace std;

// 原作者设定的 后缀集合
const unordered_map<string, string> HttpResponse::SUFFIX_TYPE = {
    { ".html",  "text/html" },
    { ".xml",   "text/xml" },
    { ".xhtml", "application/xhtml+xml" },
    { ".txt",   "text/plain" },
    { ".rtf",   "application/rtf" },
    { ".pdf",   "application/pdf" },
    { ".word",  "application/nsword" },
    { ".png",   "image/png" },
    { ".gif",   "image/gif" },
    { ".jpg",   "image/jpeg" },
    { ".jpeg",  "image/jpeg" },
    { ".au",    "audio/basic" },
    { ".mpeg",  "video/mpeg" },
    { ".mpg",   "video/mpeg" },
    { ".avi",   "video/x-msvideo" },
    { ".gz",    "application/x-gzip" },
    { ".tar",   "application/x-tar" },
    { ".css",   "text/css "},
    { ".js",    "text/javascript "},
};
// 状态码
const unordered_map<int, string> HttpResponse::CODE_STATUS = {
    { 200, "OK" },
    { 400, "Bad Request" },
    { 403, "Forbidden" },
    { 404, "Not Found" },
};
// 状态码对应的 html 路径
const unordered_map<int, string> HttpResponse::CODE_PATH = {
    { 400, "/400.html" },
    { 403, "/403.html" },
    { 404, "/404.html" },
};
// HttpResponse 构造函数
HttpResponse::HttpResponse() {
    code_ = -1;
    path_ = srcDir_ = "";
    isKeepAlive_ = false;
    mmFile_ = nullptr; 
    mmFileStat_ = { 0 };
};
// HttpResponse 析构函数
HttpResponse::~HttpResponse() {
    UnmapFile();  // 释放共享内存
}
// 初始化
void HttpResponse::Init(const string& srcDir, string& path, bool isKeepAlive, int code){
    assert(srcDir != "");
    if(mmFile_) { UnmapFile(); }
    code_ = code;
    isKeepAlive_ = isKeepAlive;
    path_ = path;
    srcDir_ = srcDir;
    mmFile_ = nullptr; 
    mmFileStat_ = { 0 };
}

void HttpResponse::MakeResponse(Buffer& buff) {
    // 判断请求的资源文件 是否存在，是否有权限获取
    if(stat((srcDir_ + path_).data(), &mmFileStat_) < 0 || S_ISDIR(mmFileStat_.st_mode)) {
        code_ = 404;  // 文件不存在，或者请求的是目录，不是文件
    }
    else if(!(mmFileStat_.st_mode & S_IROTH)) {
        code_ = 403;  // 无权访问该资源文件
    }
    else if(code_ == -1) { // 构造函数初始化时 code_ = -1
        code_ = 200;  
    }
    // 进入到这里时，code_ 已经设定为 200、400、403、404
    ErrorHtml_();        // 找预先写好的 ErrorHtml: 400、403、404, 并将文件信息存入 stat
    AddStateLine_(buff); // 添加状态行，并写入 buff
    AddHeader_(buff);    // 添加相应头，并写入 buff
    AddContent_(buff);   // 找到文件资源，并映射到相应的共享内存，然后将文件大小 写入 buff
}

char* HttpResponse::File() {
    return mmFile_;  // 文件内容指针
}

size_t HttpResponse::FileLen() const {
    return mmFileStat_.st_size; // 文件大小
}

void HttpResponse::ErrorHtml_() {
    if(CODE_PATH.count(code_) == 1) {
        path_ = CODE_PATH.find(code_)->second;
        stat((srcDir_ + path_).data(), &mmFileStat_);  // 获取指定路径文件或者文件夹信息
    }
}
// 添加状态行，写入到 buff
void HttpResponse::AddStateLine_(Buffer& buff) {
    string status;
    if(CODE_STATUS.count(code_) == 1) {
        status = CODE_STATUS.find(code_)->second;
    }
    else {
        code_ = 400;
        status = CODE_STATUS.find(400)->second;
    }
    // 例子：HTTP/1.1 200 OK\r\n
    buff.Append("HTTP/1.1 " + to_string(code_) + " " + status + "\r\n");
}
// 添加相应头，写入到 buff
void HttpResponse::AddHeader_(Buffer& buff) {
    buff.Append("Connection: ");
    if(isKeepAlive_) {
        buff.Append("keep-alive\r\n");
        buff.Append("keep-alive: max=6, timeout=120\r\n");
    } else{
        buff.Append("close\r\n");
    }
    buff.Append("Content-type: " + GetFileType_() + "\r\n");
    // 例子: 
    // Connection: keep-alive
    // Keep-alive: max=6, timeout=120
    // Content-type: text/html
}

void HttpResponse::AddContent_(Buffer& buff) {
    // 打开对应文件
    int srcFd = open((srcDir_ + path_).data(), O_RDONLY);
    if(srcFd < 0) { 
        ErrorContent(buff, "File NotFound!");
        return; 
    }
    // 将文件映射到内存提高文件的访问速度 MAP_PRIVATE 建立一个写入时拷贝的私有映射
    LOG_DEBUG("file path %s", (srcDir_ + path_).data());
    // 共享内存映射
    int* mmRet = (int*)mmap(0, mmFileStat_.st_size, PROT_READ, MAP_PRIVATE, srcFd, 0);
    if(*mmRet == -1) {
        ErrorContent(buff, "File NotFound!");
        return; 
    }
    // 类型转换
    mmFile_ = (char*)mmRet;
    close(srcFd);  // 关闭 srcFd，应为共享内存已经映射了，无需 srcFd 保持打开
    buff.Append("Content-length: " + to_string(mmFileStat_.st_size) + "\r\n\r\n");
    // buff 中添加 Content-length: 1000\r\n\r\n
}

void HttpResponse::UnmapFile() { // 释放共享内存
    if(mmFile_) {
        // 释放共享内存
        munmap(mmFile_, mmFileStat_.st_size);
        mmFile_ = nullptr;
    }
}
// 判断文件类型 
string HttpResponse::GetFileType_() {
    string::size_type idx = path_.find_last_of('.');
    if(idx == string::npos) {
        return "text/plain";
    }
    string suffix = path_.substr(idx);
    if(SUFFIX_TYPE.count(suffix) == 1) {
        return SUFFIX_TYPE.find(suffix)->second;
    }
    return "text/plain";
}
// 错误消息内容
void HttpResponse::ErrorContent(Buffer& buff, string message) 
{
    string body;
    string status;
    body += "<html><title>Error</title>";
    body += "<body bgcolor=\"ffffff\">";
    if(CODE_STATUS.count(code_) == 1) {
        status = CODE_STATUS.find(code_)->second;
    } else {
        status = "Bad Request";
    }
    body += to_string(code_) + " : " + status  + "\n";
    body += "<p>" + message + "</p>";
    body += "<hr><em>simpleWebServer</em></body></html>";

    buff.Append("Content-length: " + to_string(body.size()) + "\r\n\r\n");
    buff.Append(body);
}
