#include <windows.h>
#include <stdio.h>
#include <conio.h>

#define sock_strIPAdress "\x22\x01\xA8\xC0"

const char  *strHTTP_GETBegin  = "GET ";
const char  *strHTTP_GETEnd    = " HTTP/1.1\r\nHost: ";
const char  *sock_strQueryURL  = "/webif/";
const char  *pbData            = "getval?ADR=200BF3EC&DT=5&CNT=8";
const char  *strHTTP_GETFields = "\r\nAccept: */*\
                                  \r\nAccept-Language: de,en-us;g=0.7,en;q=0.3\
                                  \r\nUser-Agent: EPTel\
                                  \r\nContent-Type: text/plain\
                                  \r\nConnection: keep-alive\r\n";

char pcStrOut[5000];
int iLen = sizeof(pbData);


void BuildHttpStr(void)
{

	sprintf_s(pcStrOut, strHTTP_GETBegin);
	strcat_s(pcStrOut, (char*)&sock_strQueryURL[0]);
	strncat_s(pcStrOut, (char*)pbData, (iLen - 1 > 0) ? (iLen - 1) : (0));
	strcat_s(pcStrOut, strHTTP_GETEnd);
	strcat_s(pcStrOut, sock_strIPAdress);
	strcat_s(pcStrOut, strHTTP_GETFields);
	strcat_s(pcStrOut, "\r\n");

	//iTmp = send(iSocket, (char*)pcStrOut, strlen(pcStrOut), 0);

}