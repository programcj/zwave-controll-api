/*
 * util.c
 *
 *  Created on: 2015年12月1日
 *      Author: cj
 *      Email:593184971@qq.com
 */

#include "tools.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include<sys/types.h>
#include<dirent.h>
#include "clog.h"
#include <unistd.h>
#include <uuid/uuid.h>

/**
 * Hex 字符串转换成 到 data 中
 */
int tools_hexstring_2_data(const char *hexstring, void *data, int dlen) {
	const char *p = hexstring;
	unsigned char *v = data;
	int tmp = 0;
	while (*p && dlen) {
		if (*p == ' ') {
			p++;
			continue;
		}
		sscanf(p, "%02x", &tmp);
		*(v++) = tmp;
		dlen--;
		p += 2;
	}
	return v - (unsigned char *) data;
}

/**
 * char **buf ; 转换成的HEX字符串存放位置
 * void *data; 数据
 * int len; 数据长度
 * 例如:
 * 		char *hexstr;
 *   		tool_hex_to_string(&hexstr,data,len);
 *   		free(hexstr);
 */
void tools_hexstring_from_data(char **hex_buf, const void *hex_data, int len) {
	int i = 0;
	char *_buf;
	*hex_buf = (char*) malloc(len * 2 + len + 1);
	if (*hex_buf) {
		memset(*hex_buf, 0, len * 2 + len + 1);
		_buf = *hex_buf;
		for (i = 0; i < len; i++) {
			sprintf(_buf, "%02X ", 0x00FF & ((char*) hex_data)[i]);
			_buf += 3;
		}
	}
}

//字符串复制,判断目标字符串是否为空
char* tools_strcpy(char *des, const char *src) {
	if (src)
		return strcpy(des, src);
	return NULL;
}

extern char* tools_strncpy(char *des, const char *src, int size) {
	if (src)
		return strncpy(des, src, size);
	return NULL;
}
void tools_strcpym(char **des, const char *src) {
	if (src != NULL && strlen(src) > 0) {
		*des = (char *) malloc(strlen(src) + 1);
		memset(*des, 0, strlen(src) + 1);
		strcpy(*des, src);
	}
}

//取文件大小
unsigned long tools_file_get_size(const char *path) {
//	FILE *f = fopen(fileName, "rb");
	unsigned long filesize = 0;
//
//	if (f != NULL) {
//		fseek(f, 0, SEEK_END);
//		len = ftell(f);
//		fseek(f, 0, SEEK_SET);
//		fclose(f);
//		f = NULL;
//	}
	struct stat statbuff;
	int rc = stat(path, &statbuff);
	if (rc < 0) {
		return filesize;
	} else {
		filesize = statbuff.st_size;
	}
	return filesize;
}

//从文件中读取size大小的数据到data中
int tools_file_read(const char *fileName, int index, unsigned char *data, int size) {
	FILE *f = fopen(fileName, "rb");
	int len = 0;
	if (f != NULL) {
		fseek(f, index, SEEK_SET);
		len = fread(data, 1, size, f);
		fclose(f);
	}
	return len;
}

unsigned short tools_crc16(unsigned char *pDataAddr, int bDataLen) {
	unsigned char WorkData;
	unsigned char bitMask;
	unsigned char NewBit;
#define CRC_POLY        0x1021
#define CRC_INIT_VALUE    0x1D0F
	unsigned short crc = 0x1D0F;
	//printf("ZW_CheckCrc16: bDataLen = %u\r\n", bDataLen);
	while (bDataLen--) {
		WorkData = *pDataAddr++;
		for (bitMask = 0x80; bitMask != 0; bitMask >>= 1) {
			/* Align test bit with next bit of the message byte, starting with msb. */
			NewBit = ((WorkData & bitMask) != 0) ^ ((crc & 0x8000) != 0);
			crc <<= 1;
			if (NewBit) {
				crc ^= CRC_POLY;
			}
		} /* for (bitMask = 0x80; bitMask != 0; bitMask >>= 1) */
	}
	return crc;
}

unsigned short tools_file_crc16(const char *fileName) {
	FILE *f = fopen(fileName, "rb");
	int len = 0;
	unsigned char *data = NULL;
	unsigned short crc16 = 0;

	if (f != NULL) {
		fseek(f, 0, SEEK_END);
		len = ftell(f);
		fseek(f, 0, SEEK_SET);
		if (len > 0) {
			data = (unsigned char*) malloc(len + 1);
			memset(data, 0, len + 1);
			fread(data, 1, len, f);
			crc16 = tools_crc16(data, len);
			free(data);
		}
		fclose(f);
		f = NULL;
	}
	return crc16;
}

int tools_md5sum_calc(const char *filePath, char md5str[33]) {
	FILE *fp = NULL;
	char cmdLine[200];
	char buffer[33];
	memset(cmdLine, 0, sizeof(cmdLine));
	strcpy(cmdLine, "md5sum ");
	strcat(cmdLine, filePath);

	fp = popen(cmdLine, "r");
	memset(buffer, 0, sizeof(buffer));
	while (fgets(buffer, sizeof(buffer), fp) != NULL) {
		break;
	}
	pclose(fp);
	if (strlen(buffer) == 0) {
		return -1;
	}
	strcpy(md5str, buffer);
	return 0;
}

int tools_readcmdline(const char *cmdline, char *buff, int maxsize) {
	FILE *fp = NULL;
	int readlen = 0;
	int linelen = 0;
	char *next = NULL;

	fp = popen(cmdline, "r");

	while (fgets(buff, maxsize - readlen, fp) != NULL && readlen < maxsize) {
		linelen = strlen(buff);
		if (linelen == 0) {
			break;
		}
		next = strchr(buff, '\n');
		if (next)
			*next = 0;
		readlen += linelen;
		buff += linelen;
	}
	pclose(fp);
	return 0;
}

int tools_read_file(const char *fileName, char **buf) {
	int result = -1;
	FILE *fp;
	fp = fopen(fileName, "r");
	if (NULL != fp) {
		int len;

		fseek(fp, 0, SEEK_END); //将文件内部指针放到文件最后面
		len = ftell(fp); //读取文件指针的位置，得到文件字符的个数
		if (len > 0) {
			*buf = (char *) malloc(len);
			if (*buf != NULL) {
				fseek(fp, 0, SEEK_SET);
				fread(*buf, len, 1, fp);
				result = 0;
			}
		}
		fclose(fp);
	}
	fp = NULL;
	return result;
}

int tools_read_file_tobuff(const char *filename, char *buff, int maxsize) {
	int result = -1;
	FILE *fp;
	fp = fopen(filename, "r");
	if (NULL != fp) {
		if (buff != NULL) {
			fseek(fp, 0, SEEK_SET);
			fread(buff, maxsize, 1, fp);
			result = 0;
		}
		fclose(fp);
	}
	fp = NULL;
	return result;
}

int tools_write_file(const char *fileName, const char *buf) {
	int result = -1;
	FILE *fp;
	fp = fopen(fileName, "w+");
	if (NULL != fp) {
		fwrite(buf, strlen(buf), 1, fp);
		result = 0;
	}
	fclose(fp);
	fp = NULL;
	return result;
}

//文件是否存在,存在返回>0;  不存在返回0
int tools_file_exists(const char *fileName) {
	return 0 == access(fileName, F_OK) ? 1 : 0;
}

void tools_uuid(char str[33]) {
	int i = 0, j = 0;
	uuid_t uuid;
	char uuidStr[37];
	memset(uuidStr, 0, sizeof(uuidStr));

	uuid_generate(uuid);
	uuid_unparse(uuid, uuidStr);
	while (i < 36) {
		if (uuidStr[i] != '-')
			uuidStr[j++] = uuidStr[i++];
		else
			i++;
	}
	uuidStr[j] = '\0';
	strncpy(str, uuidStr, 33);
	str[32] = '\0';
}

//1 000 000 (百万)微秒 = 1秒
//毫秒当前时间 类拟java的System.currentTimeMillis();
long long tools_current_time_millis() {
	struct timeval tv;
	long long t;
	gettimeofday(&tv, NULL);
	t = tv.tv_sec;
	t *= 1000;
	t += tv.tv_usec / 1000;
	return t;
}

void tools_timeget_local(char *timestring) {
	time_t timep;
//	struct tm *p;
	struct tm t;
	time(&timep);
	localtime_r(&timep, &t);
//	p = localtime(&timep); /*取得当地时间*/
	//sprintf(timestring, "%04d-%02d-%02d %02d:%02d:%02d", 1900 + p->tm_year, 1 + p->tm_mon, p->tm_mday, p->tm_hour, p->tm_min, p->tm_sec);

	sprintf(timestring, "%04d-%02d-%02d %02d:%02d:%02d", 1900 + t.tm_year, 1 + t.tm_mon, t.tm_mday, t.tm_hour, t.tm_min, t.tm_sec);
}

void tools_timeget_local_tim(char tim[9]) {
	time_t timep;
	struct tm t;
	time(&timep);
	localtime_r(&timep, &t);
	sprintf(tim, "%02d:%02d:%02d", t.tm_hour, t.tm_min, t.tm_sec);
}

void tools_timeget_otc(char timestring[30]) {
	time_t timep;
	//struct tm *p;
	struct tm t;
	char *wday[] = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };

	time(&timep);
	//p = gmtime(&timep);
	gmtime_r(&timep, &t);
	//sprintf(timestring, "%04d-%02d-%02d %s %02d:%02d:%02d", (1900 + p->tm_year), (1 + p->tm_mon), p->tm_mday, wday[p->tm_wday], p->tm_hour,
	//		p->tm_min, p->tm_sec);
	sprintf(timestring, "%04d-%02d-%02d %s %02d:%02d:%02d", (1900 + t.tm_year), (1 + t.tm_mon), t.tm_mday, wday[t.tm_wday], t.tm_hour,
			t.tm_min, t.tm_sec);
}
