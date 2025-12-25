# OpenAI C++ library 

[![Language](https://img.shields.io/badge/language-C++-blue.svg)](https://isocpp.org/)  [![Standard](https://img.shields.io/badge/c%2B%2B-11-blue.svg)](https://en.wikipedia.org/wiki/C%2B%2B#Standardization) [![License](https://img.shields.io/github/license/mashape/apistatus.svg)](https://opensource.org/licenses/MIT) ![Github worflow](https://github.com/olrea/openai-cpp/actions/workflows/cmake.yml/badge.svg)
 [![GitHub version](https://badge.fury.io/gh/olrea%2Fopenai-cpp.svg)](https://github.com/olrea/openai-cpp/releases) 

## A lightweight header only modern C++ library

OpenAI-C++ library is a **community-maintained** library which provides convenient access to the [OpenAI API](https://openai.com/api/) from applications written in the C++ language.  
The library is small with two header files (only one if you already use Nlohmann Json).

## Requirements

No special requirement. You should already have these :

+ C++11/C++14/C++17/C++20 compatible compiler
+ [libcurl](https://curl.se/libcurl/) (check [Install curl](https://everything.curl.dev/get) to make sure you have the development package)

## OpenAI C++ current implementation

The library should implement all requests on [OpenAI references](https://platform.openai.com/docs/api-reference). If any are missing (due to an update), feel free to open an issue.

| API reference | Method | Example file |
| --- | --- | --- |
| API models | [List models](https://platform.openai.com/docs/api-reference/models/list) ✅ | [1-model.cpp](examples/01-model.cpp) |
| API models | [Retrieve model](https://platform.openai.com/docs/api-reference/models/retrieve) ✅ | [1-model.cpp](examples/01-model.cpp) |
| API completions | [Create completion](https://platform.openai.com/docs/api-reference/completions/create) ✅ | [2-completion.cpp](examples/02-completion.cpp) |
| API edits | [Create completion](https://platform.openai.com/docs/api-reference/completions/create) | [3-edit.cpp](examples/03-edit.cpp) |
| API images | [Create image](https://platform.openai.com/docs/api-reference/images) ✅ | [4-image.cpp](examples/04-image.cpp) |
| API images | [Create image edit](https://platform.openai.com/docs/api-reference/images/create-edit) ✅ | [4-image.cpp](examples/04-image.cpp) |
| API images | [Create image variation](https://platform.openai.com/docs/api-reference/images/create-variation) ✅ | [4-image.cpp](examples/04-image.cpp) |
| API embeddings | [Create embeddings](https://platform.openai.com/docs/api-reference/embeddings/create) ✅ | [5-embedding.cpp](examples/05-embedding.cpp) |
| API files | [List file](https://platform.openai.com/docs/api-reference/files/list) ✅ | [6-file.cpp](examples/06-file.cpp) |
| API files | [Upload file](https://platform.openai.com/docs/api-reference/files/upload) ✅ | [6-file.cpp](examples/06-file.cpp) |
| API files | [Delete file](https://platform.openai.com/docs/api-reference/files/delete) ✅ | [6-file.cpp](examples/06-file.cpp) |
| API files | [Retrieve file](https://platform.openai.com/docs/api-reference/files/retrieve) ✅ | [6-file.cpp](examples/06-file.cpp) |
| API files | [Retrieve file content](https://platform.openai.com/docs/api-reference/files/retrieve-content) ✅ | [6-file.cpp](examples/06-file.cpp) |
| API fine-tunes | [Create fine-tune](https://platform.openai.com/docs/api-reference/fine-tunes/create) ✅ | [7-fine-tune.cpp](examples/07-fine-tune.cpp) |
| API fine-tunes | [List fine-tune](https://platform.openai.com/docs/api-reference/fine-tunes/list) ✅ | [7-fine-tune.cpp](examples/07-fine-tune.cpp) |
| API fine-tunes | [Retrieve fine-tune](https://platform.openai.com/docs/api-reference/fine-tunes/retrieve) ✅ | [7-fine-tune.cpp](examples/07-fine-tune.cpp) |
| API fine-tunes | [Cancel fine-tune](https://platform.openai.com/docs/api-reference/fine-tunes/cancel) ✅ | [7-fine-tune.cpp](examples/07-fine-tune.cpp) |
| API fine-tunes | [List fine-tune events](https://platform.openai.com/docs/api-reference/fine-tunes/events) ✅ | [7-fine-tune.cpp](examples/07-fine-tune.cpp) |
| API fine-tunes | [Delete fine-tune model](https://platform.openai.com/docs/api-reference/fine-tunes/delete-model) ✅ | [7-fine-tune.cpp](examples/07-fine-tune.cpp) |
| API chat | [Create chat completion](https://platform.openai.com/docs/api-reference/chat/create) ✅ | [10-chat.cpp](examples/10-chat.cpp) |
| API audio | [Create transcription](https://platform.openai.com/docs/api-reference/audio/create) ✅ | [11-audio.cpp](examples/11-audio.cpp) |
| API audio | [Create translation](https://platform.openai.com/docs/api-reference/audio/create) ✅ | [11-audio.cpp](examples/11-audio.cpp) |
| API moderation | [Create moderation](https://platform.openai.com/docs/api-reference/moderations/create) ✅ | [12-moderation.cpp](examples/11-moderation.cpp) |

## Installation

The library consists of three files: 
https://github.com/xhqm-xyz/openai-cpp
https://github.com/yhirose/cpp-httplib
https://github.com/nlohmann/json

## License

[MIT](LICENSE.md)


## 对比

基于httplib的版本优势：

1. 代码简洁性和现代性
• 使用C++11/14现代特性，代码更简洁
• 智能指针管理资源，减少内存泄漏风险
• 更符合现代C++编程风格

2. 依赖管理
• 只需单个头文件依赖，更容易集成
• 无需预编译库，构建更简单

3. 错误处理
• 异常安全性更好
• 更清晰的错误信息传递

4. 性能
• 更轻量级的实现
• 减少系统调用开销
• 更好的连接复用


基于libcurl的版本优势：

1. 成熟度和稳定性
• libcurl是经过长期验证的稳定库
• 支持更多HTTP协议特性
• 更好的SSL/TLS支持

2. 功能完整性
• 支持更多高级HTTP特性
• 更好的代理支持
• 更完善的multipart/form-data处理

3. 平台兼容性
• 在旧系统上兼容性更好
• 对Windows平台支持更成熟
• 更好的SSL证书处理

4. 调试和监控
• 更详细的错误信息
• 更好的调试支持
• 更成熟的性能优化




