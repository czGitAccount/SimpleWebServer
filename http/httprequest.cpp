#include "httprequest.h"
using namespace std;

const unordered_set<string> HttpRequest::DEFAULT_HTML{"/index"}; // 目前只复现了一个

void HttpRequest::Init() {
    method_ = path_ = version_ = body_ = "";
    state_ = REQUEST_LINE;  // Line 有限状态机 从 REQUEST_LINE 状态开始
    header_.clear();
    // post_.clear();
}
// 判断是否保持连接
bool HttpRequest::IsKeepAlive() const {
    if(header_.count("Connection") == 1) { // keep-alive 保持连接选项, 且需要 HTTP 1.1 版本支持
        return header_.find("Connection")->second == "keep-alive" && version_ == "1.1";
    }
    return false;
}

bool HttpRequest::parse(Buffer& buff) {
    const char CRLF[] = "\r\n";  // 定义空行
    if(buff.ReadableBytes() <= 0) {
        return false;
    }
    // 从 Buffer 中解析内容
    while(buff.ReadableBytes() && state_ != FINISH) {
        // [readPos_ write_Pos)中查找第一次出现 CRLF 的位置，每次按照行读取。
        const char* lineEnd = search(buff.Peek(), buff.BeginWriteConst(), CRLF, CRLF + 2);
        // 取出该行。
        std::string line(buff.Peek(), lineEnd);
        switch(state_)  // 状态转移
        {
        // 初始状态从 REQUEST_LINE 开始
        case REQUEST_LINE:
            // 通过正则表达式解析 request 并将其对应的元素分组
            // 成功后，会有状态转移： REQUEST_LINE --> HEADERS
            if(!ParseRequestLine_(line)) {
                return false;
            }
            // 解析请求资源 name
            ParsePath_();
            break;
        // 请求行解析完成后，进一步解析 header    
        case HEADERS:
            ParseHeader_(line);
            // 如果可读空间不足，说明没有解析内容了
            if(buff.ReadableBytes() <= 2) {
                state_ = FINISH;
            }
            break;
        case BODY:
            ParseBody_(line);
            break;
        default:
            break;
        }
        // 缓冲区没有可读数据 --> 解析完成
        if(lineEnd == buff.BeginWrite()) { break; }
        // Buffer 清除解析后的行并连同清除 CRLF
        // 该过程中 readPos 会移动到 CRLF 后第一个位置
        buff.RetrieveUntil(lineEnd + 2);
    }
    LOG_DEBUG("[%s], [%s], [%s]", method_.c_str(), path_.c_str(), version_.c_str());
    return true;
}

void HttpRequest::ParsePath_() {
    // 只复现了首页, 另外可以测试一下状态码页面
    if(path_ == "/") {
        path_ = "/welcome.html"; 
    }
    else if (path_ == "/400") {
        path_ += ".html"; 
    }
    else if (path_ == "/403") {
        path_ += ".html"; 
    }
    else if (path_ == "/404") {
        path_ += ".html"; 
    }
}

bool HttpRequest::ParseRequestLine_(const string& line) {
    // 匹配字符串： |任意非空 http(s):任意非空 HTTP/任意非空|   --> 三个group ()  ()  ()
    // 例子：       GET / HTTP/1.1
    // 例子：       GET /404 HTTP/1.1
    regex patten("^([^ ]*) ([^ ]*) HTTP/([^ ]*)$"); // 正则表达式解析请求行
    smatch subMatch;
    if(regex_match(line, subMatch, patten)) {   
        method_ = subMatch[1];    // 请求方法 
        path_ = subMatch[2];      // URL
        version_ = subMatch[3];   // HTTP 版本号
        state_ = HEADERS;         // 状态转移
        return true;
    }
    LOG_ERROR("RequestLine Error");
    return false;
}

void HttpRequest::ParseHeader_(const string& line) {
    // 匹配字符串： |(任意非:):0个或1个空白字符(任意字符)|  --> 两个group () ()
    // 例子：       connection: keep-alive
    // 例子：       Host: 8.8.8.8
    regex patten("^([^:]*): ?(.*)$"); // 正则表达式解析请求头
    smatch subMatch;
    if(regex_match(line, subMatch, patten)) {
        // 使用 hashmap 按照 KV 存储了对应关系
        header_[subMatch[1]] = subMatch[2];
    }
    // header 已经解析完了，状态转移：HEADERS --> BODY
    else {
        state_ = BODY;
    }
}

void HttpRequest::ParseBody_(const string& line) {
    body_ = line;
    // ParsePost_();   // 没有复现 Post
    state_ = FINISH;   // 状态结束
    LOG_DEBUG("Body:%s, len:%d", line.c_str(), line.size());
}

// 没有复现 post
// void HttpRequest::ParsePost_() {}

std::string HttpRequest::path() const{
    return path_;
}

std::string& HttpRequest::path(){
    return path_;
}
std::string HttpRequest::method() const {
    return method_;
}

std::string HttpRequest::version() const {
    return version_;
}