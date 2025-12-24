// The MIT License (MIT)
// 
// Copyright (c) 2023 Olrea, Florian Dang
// 
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
// 
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#ifndef OPENAI_HPP_
#define OPENAI_HPP_

#if OPENAI_VERBOSE_OUTPUT
#pragma message ("OPENAI_VERBOSE_OUTPUT is ON")
#endif

#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
#include <sstream>
#include <mutex>
#include <cstdlib>
#include <map>
#include <memory>

#define CPPHTTPLIB_OPENSSL_SUPPORT
#include "nlohmann/json.hpp"
#include "httplib.h"

namespace openai {

namespace _detail {

	inline std::string EnvValue(const std::string& name) {
#ifdef _WIN32
		char* env_value = nullptr;
		size_t len = 0;
		if (_dupenv_s(&env_value, &len, name.c_str()) == 0 && env_value != nullptr) {
			std::string result(env_value);
			free(env_value);
			return result;
		}
		return "";
#else
		const char* env_value = std::getenv(name.c_str());
		return env_value ? std::string(env_value) : "";
#endif
	}

// Json alias
using Json = nlohmann::json;

struct Response {
    std::string text;
    bool        is_error;
    std::string error_message;
};

// 使用httplib替换curl的Session类
class Session {
public:
    Session(bool throw_exception, std::string proxy_url = "") : throw_exception_{throw_exception} {
        ignore_ssl_ = true;
        if (proxy_url != "") {
            setProxyUrl(proxy_url);
        }
    }

    ~Session() = default;

    // 保持原接口
    void ignoreSSL() {
        ignore_ssl_ = true;
        if (client_) {
#ifdef CPPHTTPLIB_OPENSSL_SUPPORT
            // 如果是HTTPS客户端，设置不验证证书
            client_->enable_server_certificate_verification(false);
#endif
        }
    }

    void setUrl(const std::string& url) {
        url_ = url;
        auto [http,host,post,path] = parseUrl(url_);
        if (http_ != http || host_ != host || post_ != post) {
            client_.reset(); // 重置客户端，下次请求时重新创建
        }
        http_ = http; host_ = host; post_ = post; path_ = path;
    }

    void setToken(const std::string& token, const std::string& organization) {
        token_ = token;
        organ_ = organization;
    }

    void setProxyUrl(const std::string& url) {
        proxy_ = url;
        client_.reset(); // 重置客户端，下次请求时重新创建
    }

    void setBeta(const std::string& beta) { 
        beta_ = beta; 
    }

    void setBody(const std::string& data) {
        body_ = data;
        use_multipart_ = false; // 使用body时禁用multipart
    }

    void setMultiformPart(const std::pair<std::string, std::string>& filefield_and_filepath, const std::map<std::string, std::string>& fields) {
        file_field_ = filefield_and_filepath;
        form_fields_ = fields;
        use_multipart_ = true;
        body_.clear();
    }

    Response getPrepare();
    Response postPrepare(const std::string& contentType = "");
    Response deletePrepare();
    Response makeRequest(const std::string& contentType = "");
    std::string easyEscape(const std::string& text);

private:
    // 内部辅助方法
    static httplib::Client* setupClientOptions(Session* cli);
    static std::array<std::string, 4> parseUrl(const std::string& url);

    // 原curl版中的回调函数，这里保留但改为httplib实现
    static size_t writeFunction(void* ptr, size_t size, size_t nmemb, std::string* data) {
        data->append((char*)ptr, size * nmemb);
        return size * nmemb;
    }

private:
    std::unique_ptr<httplib::Client> client_;
    std::string url_;
    std::string http_;
    std::string host_;
    std::string post_;
    std::string path_;
    std::string proxy_;
    std::string token_;
    std::string organ_;
    std::string beta_;
    std::string body_;

    // 多部分表单数据
    bool ignore_ssl_ = false;
    bool use_multipart_ = false;
    bool throw_exception_ = false;
    std::pair<std::string, std::string> file_field_;
    std::map<std::string, std::string> form_fields_;
    std::mutex  mutex_request_;
    std::string prepare_;
};

inline httplib::Client* Session::setupClientOptions(Session* cli) {
    if (!cli->client_) {
        if (cli->http_.empty() || cli->host_.empty() || cli->post_.empty()) {
            throw std::runtime_error("URL not set");
        }

        // 创建客户端
        cli->client_ = std::make_unique<httplib::Client>(cli->http_ + "://" + cli->host_ + ":" + cli->post_);
        if (cli->http_ == "https") {
            if (cli->ignore_ssl_) {
                cli->client_->enable_server_certificate_verification(false);
            }
        }
    }

    if (cli->client_) {
        // 设置超时
        cli->client_->set_connection_timeout(30, 0); // 30秒连接超时
        cli->client_->set_read_timeout(60, 0);       // 60秒读取超时
        cli->client_->set_write_timeout(60, 0);      // 60秒写入超时

        // 设置代理
        if (!cli->proxy_.empty()) {
            size_t protocol_pos = cli->proxy_.find("://");
            std::string proxy_url = cli->proxy_;
            if (protocol_pos != std::string::npos) {
                proxy_url = cli->proxy_.substr(protocol_pos + 3);
            }

            size_t colon_pos = proxy_url.find(":");
            if (colon_pos != std::string::npos) {
                std::string proxy_host = proxy_url.substr(0, colon_pos);
                int proxy_port = std::stoi(proxy_url.substr(colon_pos + 1));
                cli->client_->set_proxy(proxy_host, proxy_port);
            }
        }
    }

    return cli->client_.get();
}

inline std::array<std::string, 4> Session::parseUrl(const std::string& url) {
    // 解析 URL = 协议://主机:端口/路径 -> {协议,主机,端口,路径}
    std::array<std::string, 4> result = { "", "", "", "" };

    // 查找协议部分
    size_t protocol_end = url.find("://");
    if (protocol_end != std::string::npos) {
        result[0] = url.substr(0, protocol_end);  // 协议
        size_t host_start = protocol_end + 3;  // 跳过"://"

        // 查找主机和端口部分
        size_t port_start = url.find(':', host_start);
        size_t path_start = url.find('/', host_start);

        if (port_start != std::string::npos && (path_start == std::string::npos || port_start < path_start)) {
            // 有端口号
            result[1] = url.substr(host_start, port_start - host_start);  // 主机

            if (path_start != std::string::npos) {
                result[2] = url.substr(port_start + 1, path_start - port_start - 1);  // 端口
                result[3] = url.substr(path_start);  // 路径
            }
            else {
                result[2] = url.substr(port_start + 1);  // 端口
                result[3] = "/";  // 路径
            }
        }
        else if (path_start != std::string::npos) {
            // 没有端口号，但有路径
            result[1] = url.substr(host_start, path_start - host_start);  // 主机
            result[2] = (result[0] == "http" ? "80" : "443");  // 端口
            result[3] = url.substr(path_start);  // 路径
        }
        else {
            // 没有端口号和路径
            result[1] = url.substr(host_start);  // 主机
            result[2] = (result[0] == "http" ? "80" : "443");  // 端口
            result[3] = "/";  // 路径
        }
    }

    return result;
}


// 公有接口实现
inline Response Session::getPrepare() {
    prepare_ = "GET";
    return makeRequest();
}

inline Response Session::postPrepare(const std::string& contentType) {
    prepare_ = "POST";
    return makeRequest(contentType);
}

inline Response Session::deletePrepare() {
    prepare_ = "DELETE";
    return makeRequest();
}

inline Response Session::makeRequest(const std::string& contentType) {
    std::lock_guard<std::mutex> lock(mutex_request_);

    try {
        auto* client = setupClientOptions(this);
        if (!client) {
            return { "", true, "Client not initialized" };
        }

        // 构建headers
        httplib::Headers headers = {
            {"Authorization", "Bearer " + token_}
        };

        if (!organ_.empty()) {
            headers.emplace("OpenAI-Organization", organ_);
        }

        if (!beta_.empty()) {
            headers.emplace("OpenAI-Beta", beta_);
        }

        if (!contentType.empty()) {
            headers.emplace("Content-Type", contentType);
            if (contentType == "multipart/form-data") {
                headers.emplace("Expect", "");
            }
        }

        httplib::Result response;

        // 执行请求
        if (prepare_ == "GET") {
            response = client->Get(path_, headers);
        }
        else if (prepare_ == "DELETE") {
            response = client->Delete(path_, headers);
        }
        else {
            if (!use_multipart_) {
                response = client->Post(path_, headers, body_, contentType);
            }
            else {
                // 处理multipart/form-data
                httplib::MultipartFormDataItems items;

                // 添加文件
                items.push_back({
                    file_field_.first,
                    "",  // 内容为空，因为我们使用文件名
                    file_field_.second,
                    "application/octet-stream"
                    });

                // 添加其他字段
                for (const auto& field : form_fields_) {
                    items.push_back({
                        field.first,
                        field.second,
                        "",  // 无文件名
                        ""   // 无内容类型
                        });
                }

                use_multipart_ = false; // 重置标志
                response = client->Post(path_, headers, items);
            }
        }

        if (!response) {
            return { "", true, "No response from server" };
        }

        if (response->status >= 400) {
            std::string error_msg = "HTTP Error: " + std::to_string(response->status) + " - " + response->body;
            return { response->body, true, error_msg };
        }

        return { response->body, false, "" };

    }
    catch (const std::exception& e) {
        std::string error_msg = "HTTP Request failed: " + std::string(e.what());
        if (throw_exception_) {
            throw std::runtime_error(error_msg);
        }
        else {
            std::cerr << error_msg << '\n';
            return { "", true, error_msg };
        }
    }
}

inline std::string Session::easyEscape(const std::string& text) {
    return httplib::detail::encode_url(text);
}

// 前向声明
class OpenAI;

// 以下是原有的各种Category结构体定义，保持不变
struct CategoryModel {
    Json list();
    Json retrieve(const std::string& model);
    CategoryModel(OpenAI& openai) : openai_{openai} {}
private:
    OpenAI& openai_;
};

struct CategoryAssistants {
    Json create(Json input);
    Json retrieve(const std::string& assistants);
    Json modify(const std::string& assistants, Json input);
    Json del(const std::string& assistants);
    Json list();
    Json createFile(const std::string& assistants, Json input);
    Json retrieveFile(const std::string& assistants, const std::string& files);
    Json delFile(const std::string& assistants, const std::string& files);
    Json listFile(const std::string& assistants);
    CategoryAssistants(OpenAI& openai) : openai_{openai} {}
private:
    OpenAI& openai_;
};

struct CategoryThreads {
    Json create();
    Json retrieve(const std::string& threads);
    Json modify(const std::string& threads, Json input);
    Json del(const std::string& threads);
    Json list();
    Json createMessage(const std::string& threads, Json input);
    Json retrieveMessage(const std::string& threads, const std::string& messages);
    Json modifyMessage(const std::string& threads, const std::string& messages, Json input);
    Json listMessage(const std::string& threads);
    Json retrieveMessageFile(const std::string& threads, const std::string& messages, const std::string& files);
    Json listMessageFile(const std::string& threads, const std::string& messages);
    Json createRun(const std::string& threads, Json input);
    Json retrieveRun(const std::string& threads, const std::string& runs);
    Json modifyRun(const std::string& threads, const std::string& runs, Json input);
    Json listRun(const std::string& threads);
    Json submitToolOutputsToRun(const std::string& threads, const std::string& runs, Json input);
    Json cancelRun(const std::string& threads, const std::string& runs);
    Json createThreadAndRun(Json input);
    Json retrieveRunStep(const std::string& threads, const std::string& runs, const std::string& steps);
    Json listRunStep(const std::string& threads, const std::string& runs);
    CategoryThreads(OpenAI& openai) : openai_{openai} {}
private:
    OpenAI& openai_;
};

struct CategoryCompletion {
    Json create(Json input);
    CategoryCompletion(OpenAI& openai) : openai_{openai} {}
private:
    OpenAI& openai_;
};

struct CategoryChat {
    Json create(Json input);
    CategoryChat(OpenAI& openai) : openai_{openai} {}
private:
    OpenAI& openai_;
};

struct CategoryAudio {
    Json transcribe(Json input);
    Json translate(Json input);
    CategoryAudio(OpenAI& openai) : openai_{openai} {}
private:
    OpenAI& openai_;
};

struct CategoryEdit {
    Json create(Json input);
    CategoryEdit(OpenAI& openai) : openai_{openai} {}
private:
    OpenAI& openai_;
};

struct CategoryImage {
    Json create(Json input);
    Json edit(Json input);
    Json variation(Json input);
    CategoryImage(OpenAI& openai) : openai_{openai} {}
private:
    OpenAI& openai_;
};

struct CategoryEmbedding {
    Json create(Json input);
    CategoryEmbedding(OpenAI& openai) : openai_{openai} {}
private:
    OpenAI& openai_;
};

struct CategoryFile {
    Json list();
    Json upload(Json input);
    Json del(const std::string& file);
    Json retrieve(const std::string& file_id);
    Json content(const std::string& file_id);
    CategoryFile(OpenAI& openai) : openai_{openai} {}
private:
    OpenAI& openai_;
};

struct CategoryFineTune {
    Json create(Json input);
    Json list();
    Json retrieve(const std::string& fine_tune_id);
    Json content(const std::string& fine_tune_id);
    Json cancel(const std::string& fine_tune_id);
    Json events(const std::string& fine_tune_id);
    Json del(const std::string& model);
    CategoryFineTune(OpenAI& openai) : openai_{openai} {}
private:
    OpenAI& openai_;
};

struct CategoryModeration {
    Json create(Json input);
    CategoryModeration(OpenAI& openai) : openai_{openai} {}
private:
    OpenAI& openai_;
};

// OpenAI
class OpenAI {
public:
    OpenAI(const std::string& token = "", const std::string& organization = "", bool throw_exception = true, const std::string& api_base_url = "", const std::string& beta = "")
        : session_{ throw_exception }, token_{ token }, organization_{ organization }, throw_exception_{ throw_exception } {
        if (token.empty()) {
            token_ = EnvValue("OPENAI_API_KEY");
        }
        if (api_base_url.empty()) {
            base_url = EnvValue("OPENAI_API_BASE") + "/";
            if (base_url == "/") {
                base_url = "https://api.openai.com/v1/";
            }
        }
        else {
            base_url = api_base_url;
        }
        session_.setUrl(base_url);
        session_.setToken(token_, organization_);
        session_.setBeta(beta);
    }

    OpenAI(const OpenAI&) = delete;
    OpenAI& operator=(const OpenAI&) = delete;
    OpenAI(OpenAI&&) = delete;
    OpenAI& operator=(OpenAI&&) = delete;

    void setToken(const std::string& token = "", const std::string& organization = "") { session_.setToken(token, organization); }

    void setProxy(const std::string& url) { session_.setProxyUrl(url); }

    void setBeta(const std::string& beta) { session_.setBeta(beta); }

    // void change_token(const std::string& token) { token_ = token; };
    void setThrowException(bool throw_exception) { throw_exception_ = throw_exception; }

    void setMultiformPart(const std::pair<std::string, std::string>& filefield_and_filepath, const std::map<std::string, std::string>& fields) { session_.setMultiformPart(filefield_and_filepath, fields); }

    Json post(const std::string& suffix, const std::string& data, const std::string& contentType) {
        setParameters(suffix, data, contentType);
        auto response = session_.postPrepare(contentType);
        if (response.is_error) {
            trigger_error(response.error_message);
        }

        Json json{};
        if (isJson(response.text)) {

            json = Json::parse(response.text);
            checkResponse(json);
        }
        else {
#if OPENAI_VERBOSE_OUTPUT
            std::cerr << "Response is not a valid JSON";
            std::cout << "<< " << response.text << "\n";
#endif
    }

        return json;
}

    Json get(const std::string& suffix, const std::string& data = "") {
        setParameters(suffix, data);
        auto response = session_.getPrepare();
        if (response.is_error) { trigger_error(response.error_message); }

        Json json{};
        if (isJson(response.text)) {
            json = Json::parse(response.text);
            checkResponse(json);
        }
        else {
#if OPENAI_VERBOSE_OUTPUT
            std::cerr << "Response is not a valid JSON\n";
            std::cout << "<< " << response.text << "\n";
#endif
            json = Json{ {"Result", response.text} };
    }
        return json;
    }

    Json post(const std::string& suffix, const Json& json, const std::string& contentType = "application/json") {
        return post(suffix, json.dump(), contentType);
    }

    Json del(const std::string& suffix) {
        setParameters(suffix, "");
        auto response = session_.deletePrepare();
        if (response.is_error) { trigger_error(response.error_message); }

        Json json{};
        if (isJson(response.text)) {
            json = Json::parse(response.text);
            checkResponse(json);
        }
        else {
#if OPENAI_VERBOSE_OUTPUT
            std::cerr << "Response is not a valid JSON\n";
            std::cout << "<< " << response.text << "\n";
#endif
    }
        return json;
    }

    std::string easyEscape(const std::string& text) { return session_.easyEscape(text); }

    void debug() const { std::cout << token_ << '\n'; }

    void setBaseUrl(const std::string& url) {
        base_url = url;
    }

    std::string getBaseUrl() const {
        return base_url;
    }

private:
    std::string base_url;

    void setParameters(const std::string& suffix, const std::string& data, const std::string& contentType = "") {
        auto complete_url = base_url + suffix;
        session_.setUrl(complete_url);

        if (contentType != "multipart/form-data") {
            session_.setBody(data);
        }

#if OPENAI_VERBOSE_OUTPUT
        std::cout << "<< request: " << complete_url << "  " << data << '\n';
#endif
        }

    void checkResponse(const Json& json) {
        if (json.count("error")) {
            auto reason = json["error"].dump();
            trigger_error(reason);

#if OPENAI_VERBOSE_OUTPUT
            std::cerr << ">> response error :\n" << json.dump(2) << "\n";
#endif
        }
    }

    // as of now the only way
    bool isJson(const std::string& data) {
        bool rc = true;
        try {
            auto json = Json::parse(data); // throws if no json 
        }
        catch (std::exception&) {
            rc = false;
        }
        return(rc);
    }

    void trigger_error(const std::string& msg) {
        if (throw_exception_) {
            throw std::runtime_error(msg);
        }
        else {
            std::cerr << "[OpenAI] error. Reason: " << msg << '\n';
        }
    }

public:
    CategoryModel           model{ *this };
    CategoryAssistants      assistant{ *this };
    CategoryThreads         thread{ *this };
    CategoryCompletion      completion{ *this };
    CategoryEdit            edit{ *this };
    CategoryImage           image{ *this };
    CategoryEmbedding       embedding{ *this };
    CategoryFile            file{ *this };
    CategoryFineTune        fine_tune{ *this };
    CategoryModeration      moderation{ *this };
    CategoryChat            chat{ *this };
    CategoryAudio           audio{ *this };
    // CategoryEngine          engine{*this}; // Not handled since deprecated (use Model instead)

private:
    Session                 session_;
    std::string             token_;
    std::string             organization_;
    bool                    throw_exception_;
    };

inline std::string bool_to_string(const bool b) {
	std::ostringstream ss;
	ss << std::boolalpha << b;
	return ss.str();
}

inline OpenAI& start(const std::string& token = "", const std::string& organization = "", bool throw_exception = true, const std::string& api_base_url = "") {
	static OpenAI instance{ token, organization, throw_exception, api_base_url };
	return instance;
}

inline OpenAI& instance() {
	return start();
}

inline Json post(const std::string& suffix, const Json& json) {
	return instance().post(suffix, json);
}

inline Json get(const std::string& suffix/*, const Json& json*/) {
	return instance().get(suffix);
}

// Helper functions to get category structures instance()

inline CategoryModel& model() {
	return instance().model;
}

inline CategoryAssistants& assistant() {
	return instance().assistant;
}

inline CategoryThreads& thread() {
	return instance().thread;
}

inline CategoryCompletion& completion() {
	return instance().completion;
}

inline CategoryChat& chat() {
	return instance().chat;
}

inline CategoryAudio& audio() {
	return instance().audio;
}

inline CategoryEdit& edit() {
	return instance().edit;
}

inline CategoryImage& image() {
	return instance().image;
}

inline CategoryEmbedding& embedding() {
	return instance().embedding;
}

inline CategoryFile& file() {
	return instance().file;
}

inline CategoryFineTune& fineTune() {
	return instance().fine_tune;
}

inline CategoryModeration& moderation() {
	return instance().moderation;
}

// Definitions of category methods

// GET https://api.openai.com/v1/models
// Lists the currently available models, and provides basic information about each one such as the owner and availability.
inline Json CategoryModel::list() {
	return openai_.get("models");
}

// GET https://api.openai.com/v1/models/{model}
// Retrieves a model instance, providing basic information about the model such as the owner and permissioning.
inline Json CategoryModel::retrieve(const std::string& model) {
	return openai_.get("models/" + model);
}

// POST https://api.openai.com/v1/assistants 
// Create an assistant with a model and instructions.
inline Json CategoryAssistants::create(Json input) {
	return openai_.post("assistants", input);
}

// GET https://api.openai.com/v1/assistants/{assistant_id}
// Retrieves an assistant.
inline Json CategoryAssistants::retrieve(const std::string& assistants) {
	return openai_.get("assistants/" + assistants);
}

// POST https://api.openai.com/v1/assistants/{assistant_id}
// Modifies an assistant.
inline Json CategoryAssistants::modify(const std::string& assistants, Json input) {
	return openai_.post("assistants/" + assistants, input);
}

// DELETE https://api.openai.com/v1/assistants/{assistant_id}
// Delete an assistant.
inline Json CategoryAssistants::del(const std::string& assistants) {
	return openai_.del("assistants/" + assistants);
}

// GET https://api.openai.com/v1/assistants
// Returns a list of assistants.
inline Json CategoryAssistants::list() {
	return openai_.get("assistants");
}

// POST https://api.openai.com/v1/assistants/{assistant_id}/files
// Create an assistant file by attaching a File to an assistant.
inline Json CategoryAssistants::createFile(const std::string& assistants, Json input) {
	return openai_.post("assistants/" + assistants + "/files", input);
}

// GET https://api.openai.com/v1/assistants/{assistant_id}/files/{file_id}
// Retrieves an AssistantFile.
inline Json CategoryAssistants::retrieveFile(const std::string& assistants, const std::string& files) {
	return openai_.get("assistants/" + assistants + "/files/" + files);
}

// DELETE https://api.openai.com/v1/assistants/{assistant_id}/files/{file_id}
// Delete an assistant file.
inline Json CategoryAssistants::delFile(const std::string& assistants, const std::string& files) {
	return openai_.del("assistants/" + assistants + "/files/" + files);
}

// GET https://api.openai.com/v1/assistants/{assistant_id}/files
// Returns a list of assistant files.
inline Json CategoryAssistants::listFile(const std::string& assistants) {
	return openai_.get("assistants/" + assistants + "/files");
}

// POST https://api.openai.com/v1/threads
// Create a thread.
inline Json CategoryThreads::create() {
	Json input;
	return openai_.post("threads", input);
}

// GET https://api.openai.com/v1/threads/{thread_id}
// Retrieves a thread.
inline Json CategoryThreads::retrieve(const std::string& threads) {
	return openai_.get("threads/" + threads);
}

// POST https://api.openai.com/v1/threads/{thread_id}
// Modifies a thread.
inline Json CategoryThreads::modify(const std::string& threads, Json input) {
	return openai_.post("threads/" + threads, input);
}

// DELETE https://api.openai.com/v1/threads/{thread_id}
// Delete a thread.
inline Json CategoryThreads::del(const std::string& threads) {
	return openai_.del("threads/" + threads);
}

// POST https://api.openai.com/v1/threads/{thread_id}/messages
// Create a message.
inline Json CategoryThreads::createMessage(const std::string& threads, Json input) {
	return openai_.post("threads/" + threads + "/messages", input);
}

// GET https://api.openai.com/v1/threads/{thread_id}/messages/{message_id}
// Retrieve a message.
inline Json CategoryThreads::retrieveMessage(const std::string& threads, const std::string& messages) {
	return openai_.get("threads/" + threads + "/messages/" + messages);
}

// POST https://api.openai.com/v1/threads/{thread_id}/messages/{message_id}
// Modifies a message.
inline Json CategoryThreads::modifyMessage(const std::string& threads, const std::string& messages, Json input) {
	return openai_.post("threads/" + threads + "/messages/" + messages, input);
}

// GET https://api.openai.com/v1/threads/{thread_id}/messages
// Returns a list of messages for a given thread.
inline Json CategoryThreads::listMessage(const std::string& threads) {
	return openai_.get("threads/" + threads + "/messages");
}

// GET https://api.openai.com/v1/threads/{thread_id}/messages/{message_id}/files/{file_id}
// Retrieves a message file.
inline Json CategoryThreads::retrieveMessageFile(const std::string& threads, const std::string& messages, const std::string& files) {
	return openai_.get("threads/" + threads + "/messages/" + messages + "/files/" + files);
}

// GET https://api.openai.com/v1/threads/{thread_id}/messages/{message_id}/files
// Returns a list of message files.
inline Json CategoryThreads::listMessageFile(const std::string& threads, const std::string& messages) {
	return openai_.get("threads/" + threads + "/messages/" + messages + "/files");
}

// POST https://api.openai.com/v1/threads/{thread_id}/runs
// Create a run.
inline Json CategoryThreads::createRun(const std::string& threads, Json input) {
	return openai_.post("threads/" + threads + "/runs", input);
}

// GET https://api.openai.com/v1/threads/{thread_id}/runs/{run_id}
// Retrieves a run.
inline Json CategoryThreads::retrieveRun(const std::string& threads, const std::string& runs) {
	return openai_.get("threads/" + threads + "/runs/" + runs);
}

// POST https://api.openai.com/v1/threads/{thread_id}/runs/{run_id}
// Modifies a run.
inline Json CategoryThreads::modifyRun(const std::string& threads, const std::string& runs, Json input) {
	return openai_.post("threads/" + threads + "/runs/" + runs, input);
}

// GET https://api.openai.com/v1/threads/{thread_id}/runs
// Returns a list of runs belonging to a thread.
inline Json CategoryThreads::listRun(const std::string& threads) {
	return openai_.get("threads/" + threads + "/runs");
}

// POST https://api.openai.com/v1/threads/{thread_id}/runs/{run_id}/submit_tool_outputs
// When a run has the status: "requires_action" and required_action.type is submit_tool_outputs, this endpoint can be used to submit the outputs from the tool calls once they're all completed. All outputs must be submitted in a single request.
inline Json CategoryThreads::submitToolOutputsToRun(const std::string& threads, const std::string& runs, Json input) {
	return openai_.post("threads/" + threads + "/runs/" + runs + "submit_tool_outputs", input);
}

// POST https://api.openai.com/v1/threads/{thread_id}/runs/{run_id}/cancel
// Cancels a run that is in_progress.
inline Json CategoryThreads::cancelRun(const std::string& threads, const std::string& runs) {
	Json input;
	return openai_.post("threads/" + threads + "/runs/" + runs + "/cancel", input);
}

// POST https://api.openai.com/v1/threads/runs
// Create a thread and run it in one request.
inline Json CategoryThreads::createThreadAndRun(Json input) {
	return openai_.post("threads/runs", input);
}

// GET https://api.openai.com/v1/threads/{thread_id}/runs/{run_id}/steps/{step_id}
// Retrieves a run step.
inline Json CategoryThreads::retrieveRunStep(const std::string& threads, const std::string& runs, const std::string& steps) {
	return openai_.get("threads/" + threads + "/runs/" + runs + "/steps/" + steps);
}

// GET https://api.openai.com/v1/threads/{thread_id}/runs/{run_id}/steps
// Returns a list of run steps belonging to a run.
inline Json CategoryThreads::listRunStep(const std::string& threads, const std::string& runs) {
	return openai_.get("threads/" + threads + "/runs/" + runs + "/steps");
}

// POST https://api.openai.com/v1/completions
// Creates a completion for the provided prompt and parameters
inline Json CategoryCompletion::create(Json input) {
	return openai_.post("completions", input);
}

// POST https://api.openai.com/v1/chat/completions
// Creates a chat completion for the provided prompt and parameters
inline Json CategoryChat::create(Json input) {
	return openai_.post("chat/completions", input);
}

// POST https://api.openai.com/v1/audio/transcriptions
// Transcribes audio into the input language.
inline Json CategoryAudio::transcribe(Json input) {
	auto lambda = [input]() -> std::map<std::string, std::string> {
		std::map<std::string, std::string> temp;
		temp.insert({ "model", input["model"].get<std::string>() });
		if (input.contains("language")) {
			temp.insert({ "language", input["language"].get<std::string>() });
		}
		if (input.contains("prompt")) {
			temp.insert({ "prompt", input["prompt"].get<std::string>() });
		}
		if (input.contains("response_format")) {
			temp.insert({ "response_format", input["response_format"].get<std::string>() });
		}
		if (input.contains("temperature")) {
			temp.insert({ "temperature", std::to_string(input["temperature"].get<float>()) });
		}
		return temp;
	};
	openai_.setMultiformPart({ "file", input["file"].get<std::string>() },
		lambda()
	);

	return openai_.post("audio/transcriptions", std::string{ "" }, "multipart/form-data");
}

// POST https://api.openai.com/v1/audio/translations
// Translates audio into into English..
inline Json CategoryAudio::translate(Json input) {
	auto lambda = [input]() -> std::map<std::string, std::string> {
		std::map<std::string, std::string> temp;
		temp.insert({ "model", input["model"].get<std::string>() });
		if (input.contains("language")) {
			temp.insert({ "language", input["language"].get<std::string>() });
		}
		if (input.contains("prompt")) {
			temp.insert({ "prompt", input["prompt"].get<std::string>() });
		}
		if (input.contains("response_format")) {
			temp.insert({ "response_format", input["response_format"].get<std::string>() });
		}
		if (input.contains("temperature")) {
			temp.insert({ "temperature", std::to_string(input["temperature"].get<float>()) });
		}
		return temp;
	};
	openai_.setMultiformPart({ "file", input["file"].get<std::string>() },
		lambda()
	);

	return openai_.post("audio/translations", std::string{ "" }, "multipart/form-data");
}

// POST https://api.openai.com/v1/translations
// Creates a new edit for the provided input, instruction, and parameters
inline Json CategoryEdit::create(Json input) {
	return openai_.post("edits", input);
}

// POST https://api.openai.com/v1/images/generations
// Given a prompt and/or an input image, the model will generate a new image.
inline Json CategoryImage::create(Json input) {
	return openai_.post("images/generations", input);
}

// POST https://api.openai.com/v1/images/edits
// Creates an edited or extended image given an original image and a prompt.
inline Json CategoryImage::edit(Json input) {
	std::string prompt = input["prompt"].get<std::string>(); // required
	// Default values
	std::string mask = "";
	int n = 1;
	std::string size = "1024x1024";
	std::string response_format = "url";
	std::string user = "";

	if (input.contains("mask")) {
		mask = input["mask"].get<std::string>();
	}
	if (input.contains("n")) {
		n = input["n"].get<int>();
	}
	if (input.contains("size")) {
		size = input["size"].get<std::string>();
	}
	if (input.contains("response_format")) {
		response_format = input["response_format"].get<std::string>();
	}
	if (input.contains("user")) {
		user = input["user"].get<std::string>();
	}
	openai_.setMultiformPart({ "image",input["image"].get<std::string>() },
		std::map<std::string, std::string>{
			{"prompt", prompt},
			{ "mask", mask },
			{ "n", std::to_string(n) },
			{ "size", size },
			{ "response_format", response_format },
			{ "user", user }
	}
	);

	return openai_.post("images/edits", std::string{ "" }, "multipart/form-data");
}

// POST https://api.openai.com/v1/images/variations
// Creates a variation of a given image.
inline Json CategoryImage::variation(Json input) {
	// Default values
	int n = 1;
	std::string size = "1024x1024";
	std::string response_format = "url";
	std::string user = "";

	if (input.contains("n")) {
		n = input["n"].get<int>();
	}
	if (input.contains("size")) {
		size = input["size"].get<std::string>();
	}
	if (input.contains("response_format")) {
		response_format = input["response_format"].get<std::string>();
	}
	if (input.contains("user")) {
		user = input["user"].get<std::string>();
	}
	openai_.setMultiformPart({ "image",input["image"].get<std::string>() },
		std::map<std::string, std::string>{
			{"n", std::to_string(n)},
			{ "size", size },
			{ "response_format", response_format },
			{ "user", user }
	}
	);

	return openai_.post("images/variations", std::string{ "" }, "multipart/form-data");
}

inline Json CategoryEmbedding::create(Json input) {
	return openai_.post("embeddings", input);
}

inline Json CategoryFile::list() {
	return openai_.get("files");
}

inline Json CategoryFile::upload(Json input) {
	openai_.setMultiformPart({ "file", input["file"].get<std::string>() },
		std::map<std::string, std::string>{ {"purpose", input["purpose"].get<std::string>()}}
	);

	return openai_.post("files", std::string{ "" }, "multipart/form-data");
}

inline Json CategoryFile::del(const std::string& file_id) {
	return openai_.del("files/" + file_id);
}

inline Json CategoryFile::retrieve(const std::string& file_id) {
	return openai_.get("files/" + file_id);
}

inline Json CategoryFile::content(const std::string& file_id) {
	return openai_.get("files/" + file_id + "/content");
}

inline Json CategoryFineTune::create(Json input) {
	return openai_.post("fine-tunes", input);
}

inline Json CategoryFineTune::list() {
	return openai_.get("fine-tunes");
}

inline Json CategoryFineTune::retrieve(const std::string& fine_tune_id) {
	return openai_.get("fine-tunes/" + fine_tune_id);
}

inline Json CategoryFineTune::content(const std::string& fine_tune_id) {
	return openai_.get("fine-tunes/" + fine_tune_id + "/content");
}

inline Json CategoryFineTune::cancel(const std::string& fine_tune_id) {
	return openai_.post("fine-tunes/" + fine_tune_id + "/cancel", Json{});
}

inline Json CategoryFineTune::events(const std::string& fine_tune_id) {
	return openai_.get("fine-tunes/" + fine_tune_id + "/events");
}

inline Json CategoryFineTune::del(const std::string& model) {
	return openai_.del("models/" + model);
}

inline Json CategoryModeration::create(Json input) {
	return openai_.post("moderations", input);
}

} // namespace _detail

// Public interface

using _detail::OpenAI;

// instance
using _detail::start;
using _detail::instance;

// Generic methods
using _detail::post;
using _detail::get;

// Helper categories access
using _detail::model;
using _detail::assistant;
using _detail::thread;
using _detail::completion;
using _detail::edit;
using _detail::image;
using _detail::embedding;
using _detail::file;
using _detail::fineTune;
using _detail::moderation;
using _detail::chat;
using _detail::audio;

using _detail::Json;

}; // namespace openai

#endif // OPENAI_HPP_