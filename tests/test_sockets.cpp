#include <rpp/sockets.h>
#include <thread>
#include <rpp/tests.h>

using std::thread;
using namespace rpp;
using Socket = rpp::socket;

TestImpl(test_sockets)
{
    TestInitNoAutorun(test_sockets)
    {
    }

    Socket create(const char* msg, Socket&& s)
    {
        Assert(s.good() && s.connected());
        printf("%s %s\n", msg, s.name().c_str());
        return std::move(s);
    }
    Socket listen(int port)
    {
        return create("server: listening on", Socket::listen_to(port));
    }
    Socket accept(const Socket& server)
    {
        return create("server: accepted client", server.accept(5000/*ms*/));
    }
    Socket connect(const char* ip, int port)
    {
        return create("remote: connected to", Socket::connect_to(ip, port, 5000/*ms*/, AF_IPv4));
    }

    /**
     * This test simulates a very simple client - server setup
     */
    TestCase(nonblocking_sockets)
    {
        Socket server = listen(1337); // this is our server
        thread remote([=] { nonblocking_remote(); }); // spawn remote client
        Socket client = accept(server);

        // wait 1ms for a client that will never come
        Socket failClient = server.accept(1);
        Assert(failClient.bad());

        client.send("Server says: Hello!");
        sleep(500);

        string resp = client.recv_str();
        if (!Assert(resp != ""))
            printf("%s\n", resp.c_str());
        sleep(500);

        printf("server: closing down\n");
        client.close();
        server.close();
        remote.join(); // wait for remote thread to finish
    }
    void nonblocking_remote() // simulated remote endpoint
    {
        Socket server = connect("127.0.0.1", 1337);
        while (server.connected())
        {
            string resp = server.recv_str();
            if (resp != "")
            {
                printf("%s\n", resp.c_str());
                Assert(server.send("Client says: Thanks!") > 0);
            }
            sleep(1);
        }
        printf("remote: server disconnected\n");
        printf("remote: closing down\n");
    }

    TestCase(transmit_data)
    {
        printf("========= TRANSMIT DATA =========\n");

        Socket server = listen(1337);
        thread remote([=] { this->transmitting_remote(); });
        Socket client = accept(server);

        for (int i = 0; i < 10; ++i)
        {
            string data = client.recv_str();
            if (data != "")
            {
                printf("server: received %d bytes of data from client ", (int)data.length());

                size_t j = 0;
                for (; j < data.length(); ++j)
                {
                    if (data[j] != '$')
                    {
                        printf("(corrupted at position %d):\n", (int)j);
                        printf("%.*s\n", 10, &data[j]);
                        printf("^\n");
                        break;
                    }
                }
                if (j == data.length()) {
                    printf("(valid)\n");
                }
            }
            sleep(500);
        }

        printf("server: closing down\n");
        client.close();
        server.close();
        remote.join();
    }

    void transmitting_remote()
    {
        char sendBuffer[80000];
        memset(sendBuffer, '$', sizeof sendBuffer);

        Socket server = connect("127.0.0.1", 1337);
        while (server.connected())
        {
            int sentBytes = server.send(sendBuffer, sizeof sendBuffer);
            if (sentBytes > 0)
                printf("remote: sent %d bytes of data\n", sentBytes);
            else
                printf("remote: failed to send data: %s\n", Socket::last_err().c_str());
            sleep(1000);
        }
        printf("remote: server disconnected\n");
        printf("remote: closing down\n");
    }

};
