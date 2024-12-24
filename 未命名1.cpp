#include <iostream>       // C++��׼���������
#include <cstring>        // �ַ���������
#include <winsock2.h>     // Windows���������غ���
#include <ws2tcpip.h>     // Windows��չ���������
#include <thread>         // C++���߳̿�
#include <chrono>         // C++ʱ�䴦���
#include <mutex>
#define FPS
#include "FeEGELib.h"

#pragma comment(lib, "ws2_32.lib") // ����Winsock��

#define PORT 19946          // �������˿ں�
#define SERVER_IP "149.88.87.140" // ������IP��ַ

using namespace FeEGE;

std::mutex data_mutex;

bool beginWith(const std::string& s,const std::string& p){
	if(s.length() < p.length()) return false;
	for(int i = 0;i < p.length();++ i){
		if(s[i] != p[i]) return false;
	}
	return true;
}

// �ͻ�����Ϣ������
void handle_client(const std::string& message, sockaddr_in client_addr, SOCKET sockfd) {
    // ʹ�� inet_ntoa ��ȡ�ͻ���IP��ַ��ע�⣺inet_ntoa �̲߳���ȫ��
    char client_ip[INET_ADDRSTRLEN];
    snprintf(client_ip, sizeof(client_ip), "%s", inet_ntoa(client_addr.sin_addr));

	if(beginWith(message,"regid : ")){
		int id,posx,posy;
		sscanf(message.c_str(),"regid : %d;pos : (%d,%d)",&id,&posx,&posy);
		data_mutex.lock();
		push_schedule([=](){
			newElement(std::to_string(id),".\\resources\\image\\0.png",posx,posy)->show();
		});
		data_mutex.unlock();
		return;
	}
	
	if(beginWith(message,"updpos id : ")){
		int id,x,y;
		sscanf(message.c_str(),"updpos id : %d;pos : (%d,%d)",&id,&x,&y);
		data_mutex.lock();
		push_schedule([=](){
			if(getElementById(std::to_string(id)) == nullptr){
				cout<<"FAILED for "<<std::to_string(id)<<"\n";
				return;
			}
			getElementById(std::to_string(id))->move_to(x,y);
		});
		data_mutex.unlock();
//		std::cout<<"MOVE "<<id<<" to "<<x<<" "<<y<<std::endl;
		return;
	}
	
	if(beginWith(message,"delid : ")){
		std::cout<<"recv! "<<message<<"\n";
		int id;
		sscanf(message.c_str(),"delid : %d",&id);
		data_mutex.lock();
		push_schedule([=](){
			if(getElementById(std::to_string(id)) == nullptr){
				cout<<"FAILED for "<<std::to_string(id)<<"\n";
				return;
			}
			getElementById(std::to_string(id))->deletethis();
		});
		data_mutex.unlock();
		return;
	}

    // ��ӡ�յ�����Ϣ�Լ��ͻ��˵�ַ�Ͷ˿���Ϣ
    std::cout << "Received message: " << message << " from "
              << client_ip << ":" << ntohs(client_addr.sin_port) << std::endl;
}

int main() {
	init(600,400);
	char buffer[1024]; 
	memset(buffer,0,sizeof(buffer));
	std::string Name;
	std::cin>>Name;
	int id,posx,posy;
	std::string PLAYER_ID;
	
    WSADATA wsaData;                          // Winsock��ʼ�����ݽṹ
    SOCKET sockfd;                            // UDP�׽����ļ�������
    struct sockaddr_in servaddr,cliaddr;              // ��������ַ�ṹ
	bool flag = false;
	std::mutex mutex_flag;
	
    std::thread([&](){ // ��ʼ�� 
    	mutex_flag.lock();
    	// ��ʼ��Winsock
	    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
	        std::cerr << "WSAStartup failed with error: " << WSAGetLastError() << std::endl;
	        return EXIT_FAILURE;
	    }
	
	    // ����UDP�׽���
	    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) == INVALID_SOCKET) {
	        std::cerr << "Socket creation failed with error: " << WSAGetLastError() << std::endl;
	        WSACleanup();
	        return EXIT_FAILURE;
	    }
	
	    // ��ʼ����������ַ�ṹ
	    memset(&servaddr, 0, sizeof(servaddr));   // ����������ַ�ṹ����
	    memset(&cliaddr, 0, sizeof(cliaddr));
	    servaddr.sin_family = AF_INET;           // ��ַ�壺IPv4
	    servaddr.sin_port = htons(PORT);         // ���˿ں�ת��Ϊ�����ֽ���
	
	    // ��������IP��ַ���ַ���ת��Ϊ�����ֽ����滻inet_ptonΪ�����Դ��룩
	    servaddr.sin_addr.s_addr = inet_addr(SERVER_IP);
	    if (servaddr.sin_addr.s_addr == INADDR_NONE) {
	        std::cerr << "Invalid address/ Address not supported" << std::endl;
	        closesocket(sockfd);
	        WSACleanup();
	        return EXIT_FAILURE;
	    }
	    
		std::string message = "Hello query Player " + Name;          // Ҫ���͵���Ϣ
	    int n = sendto(sockfd, message.c_str(), message.size(), 0, 
	                   (struct sockaddr*)&servaddr, sizeof(servaddr));
        if (n == SOCKET_ERROR) std::cerr << "sendto failed with error: " << WSAGetLastError() << std::endl;
        else std::cout << "Sent message: " << message << std::endl;
        
        int len = sizeof(cliaddr);
	    // ���տͻ�����Ϣ
	    n = recvfrom(sockfd, buffer, sizeof(buffer) - 1, 0, 
	                     (struct sockaddr*)&cliaddr, &len);
	    if (n == SOCKET_ERROR) std::cerr << "recvfrom failed with error: " << WSAGetLastError() << std::endl;
	    buffer[n] = '\0';
	    std::string raw = std::string(buffer);
	    sscanf(raw.c_str(),"id : %d;pos : (%d,%d)",&id,&posx,&posy);
	    PLAYER_ID = std::to_string(id);

	    flag = true;
	    mutex_flag.unlock();
	}).detach();
	

	auto upd_pos = [&](Element* e){
//		std::cout<<e<<"\n";
		if(e == nullptr) return;
		static int posx,posy;
//		std::cout<<"AWA\n";
		if((int)(e->get_position().x) == posx && (int)(e->get_position().y) == posy) return;
		posx = (int)(e->get_position().x);		posy = (int)(e->get_position().y);
		
		std::string message = "updpos id : " + std::to_string(id) + ";pos : (" + std::to_string(posx) + "," + std::to_string(posy) + ")";
		int n = sendto(sockfd, message.c_str(), message.size(), 0, 
	                   (struct sockaddr*)&servaddr, sizeof(servaddr));
        if (n == SOCKET_ERROR) std::cerr << "sendto failed with error: " << WSAGetLastError() << std::endl;
	};
	
	function<void(Element*)> func = [&](Element* e){
		int x,y;
	    mousepos(&x,&y);
		e->set_variable(10,e->get_position().x - x);
		e->set_variable(11,e->get_position().y - y);
		e->listen(EventType.frame,"move",[&](Element* e){
    		int x,y;
	    	mousepos(&x,&y);
	    	e->set_posx(x + e->get_variable(10));
	    	e->set_posy(y + e->get_variable(11));
		});
		e->listen(EventType.on_click,"stop",[&](Element* e){
			push_schedule([&](){
				getElementById(PLAYER_ID)->stop(EventType.frame,"move");
				getElementById(PLAYER_ID)->stop(EventType.on_click,"stop");
			});
			e->listen(EventType.on_mouse_hitting,"hit",func);
		});
		push_schedule([&](){
			getElementById(PLAYER_ID)->stop(EventType.on_mouse_hitting,"hit");
		});
	};
	
	Element* player = nullptr;
	
    // �����������ݵ�������
    std::thread([&](){
    	while(true){
    		mutex_flag.lock();
    		if(flag){
    			mutex_flag.unlock();
    			break;
			}
			mutex_flag.unlock();
		}
    	int cnt = 0;
    	
    	player = newElement(PLAYER_ID,".\\resources\\image\\0.png",posx,posy);
    	player->show();
    	player->listen(EventType.on_mouse_hitting,"hit",func);
    	
	    while (true) {
	    	std::string s;
	    	std::cin>>s;
	    	
	        const char* message = s.c_str();          // Ҫ���͵���Ϣ
	        cnt ++;
	        int n = sendto(sockfd, message, strlen(message), 0, 
	                       (struct sockaddr*)&servaddr, sizeof(servaddr));
	
	        if (n == SOCKET_ERROR) {
	            std::cerr << "sendto failed with error: " << WSAGetLastError() << std::endl;
	            break;
	        }
	
	        std::cout << "Sent message: " << message << std::endl;
	
	        // ÿ10�뷢��һ��
//	        std::this_thread::sleep_for(std::chrono::seconds(1));
	    }
	}).detach();
	
	thread([&](){
		while(true){
			upd_pos(getElementById(PLAYER_ID));
			std::this_thread::sleep_for(std::chrono::milliseconds(15));
		}
	}).detach();
	
	std::thread([&](){
		while(true){
    		mutex_flag.lock();
    		if(flag){
    			mutex_flag.unlock();
    			break;
			}
			mutex_flag.unlock();
		}
		std::cout<<flag<<"\n";
    	// ����ѭ���������Է���˵���Ϣ
	    while (true) {
	        int len = sizeof(cliaddr);          // �ͻ��˵�ַ�ṹ����
	
	        // ���տͻ�����Ϣ
	        int n = recvfrom(sockfd, buffer, sizeof(buffer) - 1, 0, 
	                         (struct sockaddr*)&cliaddr, &len);
	        if (n == SOCKET_ERROR) {
	            std::cerr << "recvfrom failed with error: " << WSAGetLastError() << std::endl;
	            continue;                       // ��������ѭ��
	        }
	
	        buffer[n] = '\0';                  // ȷ�����յ�����Ϣ��\0��β
	        
	        // ���������̴߳���ͻ�������
	        std::thread(handle_client, std::string(buffer), cliaddr, sockfd).detach();
	        // detach() ʹ�̶߳������У����߳�����ȴ������
	    }
	}).detach();
	
	std::thread([&](){
		while(true){
    		mutex_flag.lock();
    		if(flag){
    			mutex_flag.unlock();
    			break;
			}
			mutex_flag.unlock();
		}
	    while (true) {
	    	//std::this_thread::sleep_for(std::chrono::seconds(2));
	        const char* message = "KEEPACTIVE";          // Ҫ���͵���Ϣ
	        int n = sendto(sockfd, message, strlen(message), 0, 
	                       (struct sockaddr*)&servaddr, sizeof(servaddr));
	
	        if (n == SOCKET_ERROR) {
	            std::cerr << "sendto failed with error: " << WSAGetLastError() << std::endl;
	            break;
	        }
	        
//	        std::cout << "Sent message: " << message << std::endl;
	
	        // ÿ�뷢��һ��
	        std::this_thread::sleep_for(std::chrono::milliseconds(300));
	    }
	}).detach();
	
//	while(true) ;
	start();

    closesocket(sockfd);                     // �ر��׽���
    WSACleanup();                            // ����Winsock
    return 0;                                // ����0��ʾ�����˳�
}

