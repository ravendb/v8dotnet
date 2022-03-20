#include "websocket_server.h"

WebSocketServer::WebSocketServer(int port, std::function<void(std::string)> onMessage)
{
    port_ = port;
    onMessage_ = std::move(onMessage);
}

void WebSocketServer::run() {
    try
    {
        auto const address = net::ip::make_address("127.0.0.1");
        net::io_context ioc{1};
        tcp::acceptor acceptor{ioc, {address, static_cast<unsigned short>(port_)}};
        printListeningMessage();

        std::cout << "creating tcp socket" << std::endl;
        tcp::socket socket{ioc};
        std::cout << "tcp socket created" << std::endl;
        acceptor.accept(socket);
        std::cout << "sockect accepted" << std::endl;
        ws_ = std::unique_ptr<websocket::stream<tcp::socket>>(new websocket::stream<tcp::socket>(std::move(socket)));
        startListening();

        std::cout << "WebSocket based Inspector Agent finished" << std::endl;
    }
    catch (const std::exception& e)
    {
        std::cerr << "Error: " << e.what() << std::endl;
    }
}

void WebSocketServer::sendMessage(const std::string &message)
{
    try {
        boost::beast::multi_buffer b;
        boost::beast::ostream(b) << message;

        ws_->text(ws_->got_text());
        ws_->write(b.data());
    } catch(beast::system_error const& se) {
        if (se.code() != websocket::error::closed)
            std::cerr << "Error: " << se.code().message() << std::endl;
    } catch(std::exception const& e)
    {
        std::cerr << "Error: " << e.what() << std::endl;
    }
}

void WebSocketServer::startListening()
{
    try {
        std::cout << "WebSocketServer: accepting" << std::endl;
        ws_->accept();
        std::cout << "WebSocketServer: accepted" << std::endl;
        while (true) {
            waitFrontendMessage();
        }
    } catch(beast::system_error const& se) {
        if(se.code() != websocket::error::closed)
            std::cerr << "Error: " << se.code().message() << std::endl;
    } catch(std::exception const& e) {
        std::cerr << "Error: " << e.what() << std::endl;
    }
}

void WebSocketServer::printListeningMessage() {
    std::cout << "WebSocket based Inspector Agent started" << std::endl;
    std::cout << "Open the following link in your Chrome/Chromium browser: devtools://devtools/bundled/inspector.html?experiments=true&v8only=true&ws=127.0.0.1:" <<  port_ << std::endl;
}

void WebSocketServer::waitForFrontendMessageOnPause() {
    waitFrontendMessage();
}

void WebSocketServer::waitFrontendMessage()
{
    std::cout << "WebSocketServer: waitFrontendMessage: entry" << std::endl;
    beast::flat_buffer buffer;
    ws_->read(buffer);
    std::cout << "WebSocketServer: has read message" << std::endl;
    std::string message = boost::beast::buffers_to_string(buffer.data());
    std::cout << "WebSocketServer: message: " << message << std::endl;
    onMessage_(std::move(message));
    std::cout << "WebSocketServer: waitFrontendMessage: exit" << std::endl;
}