#include <iostream>       // C++��׼���������
#include <cstring>        // �ַ���������
#include <winsock2.h>     // Windows���������غ���
#include <ws2tcpip.h>     // Windows��չ���������
#include <thread>         // C++���߳̿�
#include <unordered_map>
#include <set>
#include <mutex>
#include <time.h>

#pragma comment(lib, "ws2_32.lib") // ����Winsock��

#define PORT 8088          // �������˿ں�

std::unordered_map<std::string,int> last_active;
std::set<std::string> active_list; 
std::mutex last_active_mutex;

bool beginWith(const std::string& s,const std::string& p){
	if(s.length() < p.length()) return false;
	for(int i = 0;i < p.length();++ i){
		if(s[i] != p[i]) return false;
	}
	return true;
}

std::unordered_map<std::string,int> ID;
std::unordered_map<int,std::string> Names;
std::unordered_map<std::string,std::string> IP;
std::unordered_map<std::string,int> X;
std::unordered_map<std::string,int> Y;
int cntID;
std::mutex data_mutex;

int queryID(const std::string& Name,const std::string& ip){
	int res;
	data_mutex.lock();
	if(ID.find(Name) == ID.end()){
		cntID ++;
		ID[Name] = cntID;
		Names[cntID] = Name;
	}
	if(ip.length() != 0) IP[ip] = Name;
	res = ID[Name];	
	data_mutex.unlock();
	return res;
}

int queryX(const std::string& Name){
	int res;
	data_mutex.lock();
	if(X.find(Name) == X.end()) X[Name] = 300;
	res = X[Name];	
	data_mutex.unlock();
	return res;
}

int queryY(const std::string& Name){
	int res;
	data_mutex.lock();
	if(Y.find(Name) == Y.end()) Y[Name] = 200;
	res = Y[Name];	
	data_mutex.unlock();
	return res;
}

void transmit(SOCKET sockfd,const std::string& message,const std::string& ignoreip){
	last_active_mutex.lock();
	for(const auto& s : active_list){
		if(s == ignoreip) continue;
		struct sockaddr_in addr;
		int pos = s.find(":");
		std::string sport = s.substr(pos + 1);
		std::string ip = s.substr(0,pos);
		addr.sin_family = AF_INET;
		addr.sin_port = htons(std::stoi(sport));
		addr.sin_addr.s_addr = inet_addr(ip.c_str());
		sendto(sockfd, message.c_str(), message.size(), 0, 
               (struct sockaddr*)&addr, sizeof(addr));
	}
	last_active_mutex.unlock();
}

// �ͻ�����Ϣ������
void handle_client(const std::string& message, sockaddr_in client_addr, SOCKET sockfd) {
    // ʹ�� inet_ntoa ��ȡ�ͻ���IP��ַ��ע�⣺inet_ntoa �̲߳���ȫ��
    char client_ip[INET_ADDRSTRLEN];
    snprintf(client_ip, sizeof(client_ip), "%s", inet_ntoa(client_addr.sin_addr));
    std::string curip = std::string(client_ip) + ":" + std::to_string(ntohs(client_addr.sin_port));
	
	if(message == "KEEPACTIVE"){
		last_active_mutex.lock();
		last_active[curip] = std::time(nullptr);
		active_list.insert(curip);
		last_active_mutex.unlock();
		return;
	}
	
	if(beginWith(message,"Hello query Player ")){
		std::string Name = message.substr(19);
		std::cout<<"Got a "<<Name<<std::endl;
		std::string res = "id : " + std::to_string(queryID(Name,curip)) + ";pos : (" + std::to_string(queryX(Name)) + "," + std::to_string(queryY(Name)) + ")";
		sendto(sockfd, res.c_str(), res.size(), 0, 
               (struct sockaddr*)&client_addr, sizeof(client_addr));
        std::thread(transmit,sockfd,"reg" + res,curip).detach();
        data_mutex.lock();
        for(const auto& s : active_list){
        	std::string r = "regid : " + std::to_string(ID[IP[s]]) + ";pos : (" + std::to_string(X[IP[s]]) + "," + std::to_string(Y[IP[s]]) + ")";
        	sendto(sockfd, r.c_str(), r.size(), 0, 
                   (struct sockaddr*)&client_addr, sizeof(client_addr));
		}
        data_mutex.unlock();
        return;
	}
	
	if(beginWith(message,"updpos id : ")){
		int id,x,y;
		sscanf(message.c_str(),"updpos id : %d;pos : (%d,%d)",&id,&x,&y);
		data_mutex.lock();
		X[Names[id]] = x;
		Y[Names[id]] = y;
		data_mutex.unlock();
		std::thread(transmit,sockfd,message,curip).detach();
		return;
	}

    // ��ӡ�յ�����Ϣ�Լ��ͻ��˵�ַ�Ͷ˿���Ϣ
    std::cout << "Received message: " << message << " from " << curip << std::endl;
	
	std::thread(transmit,sockfd,message,curip).detach();
}

int main() {
    WSADATA wsaData;                          // Winsock��ʼ�����ݽṹ
    SOCKET sockfd;                            // UDP�׽����ļ�������
    char buffer[1024];                        // ���ݻ�����
    struct sockaddr_in servaddr, cliaddr;     // �������Ϳͻ��˵�ַ�ṹ

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
    memset(&cliaddr, 0, sizeof(cliaddr));     // ���ͻ��˵�ַ�ṹ����

    // ���÷�������ַ�Ͷ˿�
    servaddr.sin_family = AF_INET;           // ��ַ�壺IPv4
    servaddr.sin_addr.s_addr = INADDR_ANY;   // �������б��ص�ַ
    servaddr.sin_port = htons(PORT);         // ���˿ں�ת��Ϊ�����ֽ���

    // ���׽��ֵ�ָ���˿ں͵�ַ
    if (bind(sockfd, (const struct sockaddr*)&servaddr, sizeof(servaddr)) == SOCKET_ERROR) {
        std::cerr << "Bind failed with error: " << WSAGetLastError() << std::endl;
        closesocket(sockfd);
        WSACleanup();
        return EXIT_FAILURE;
    }

    // ��������������ɹ���Ϣ
    std::cout << "UDP server is running on port " << PORT << std::endl;

    std::thread([&](){
    	// ����ѭ���������Կͻ��˵���Ϣ
	    while (true) {
	        int len = sizeof(cliaddr);
	        int n = recvfrom(sockfd, buffer, sizeof(buffer) - 1, 0, 
	                         (struct sockaddr*)&cliaddr, &len);
	        if (n == SOCKET_ERROR) {
	            std::cerr << "recvfrom failed with error: " << WSAGetLastError() << std::endl;
	            continue;
	        }
	        buffer[n] = '\0';
	        std::thread(handle_client, std::string(buffer), cliaddr, sockfd).detach();
	    }
	}).detach();
	
	std::thread([&](){
	    while (true) {
	    	int t = std::time(nullptr);
	    	last_active_mutex.lock();
	    	std::set<std::string> tmp;
	    	for(const auto& p : last_active){
	    		if(t - p.second >= 1){
	    			tmp.insert(p.first);
	    			std::cout<<"KILLED "<<p.first<<"\n";
				}
			}
			for(const auto& s : tmp){
				last_active.erase(s);
				active_list.erase(s);
			}
			for(const auto& s : tmp){
				std::thread(transmit,sockfd,"delid : " + std::to_string(ID[IP[s]]),"").detach();
			}
			last_active_mutex.unlock();
			std::this_thread::sleep_for(std::chrono::milliseconds(800));
	    }
	}).detach();
	
	while(true);
	
    closesocket(sockfd);
    WSACleanup();
    return 0;
}

