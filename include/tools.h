/*
 * tools.h
 *
 *  Created on: 2015年12月1日
 *      Author: cj
 *      Email:593184971@qq.com
 */

#ifndef COMMON_TOOLS_H_
#define COMMON_TOOLS_H_

#define  BYTE_2_SHORT(msb, lsb)		((0x00FF&msb)<<8| (0x00FF&lsb))

/**
 * Hex字符串转换成data
 */
extern int tools_hexstring_2_data(const char *hexstring,void *data, int dlen);

/**
 * char **buf ; 转换成的HEX字符串存放位置
 * void *data; 数据
 * int len; 数据长度
 * 例如:
 * 		char *hexstr;
 *   		tool_hex_to_string(&hexstr,data,len);
 *   		free(hexstr);
 */
extern void tools_hexstring_from_data(char **hex_buf, const void *hex_data, int len);

//字符串复制,判断src字符串是否为空
extern char* tools_strcpy(char *des,const  char *src);
//字符串复制,判断src字符串是否为空
extern char* tools_strncpy(char *des,const  char *src, int size);
//复制,判断src字符串是否为空,复制之前dsc字符串清0,长度为src的长度
extern void tools_strcpym(char **des, const char *src);

//文件是否存在,存在返回>0;  不存在返回0
extern int tools_file_exists(const char *fileName);

//取文件的大小
extern unsigned long tools_file_get_size(const char *filePath);
//从文件中读取size大小的数据到data中
extern int tools_file_read(const char *fileName,int index,unsigned char *data,int size);
//文件的crc16校验
extern unsigned short tools_file_crc16(const char *fileName);
//crc16校验字符串
extern unsigned short tools_crc16(unsigned char *buf, int length);

//32位md5sum计算
extern int tools_md5sum_calc(const char *filePath,char md5str[33]);

extern int tools_readcmdline(const char *cmdline, char *buff, int maxsize);

//读取文件到buf, 外部buf不为空时须要free内存, return 0 success, -1 err
extern int tools_read_file(const char *fileName,char **buf);
extern int tools_read_file_tobuff(const char *filename, char *buff,int maxsize);

//文件写入一个字符串 w+
extern int tools_write_file(const char *fileName,const char *buf);

extern void tools_uuid(char uuid[33]);

extern long long tools_current_time_millis();

extern void tools_timeget_local(char *timestring);

/**
 * 时:分:秒
 * %02d:%02d:%02d
 */
extern void tools_timeget_local_tim(char tim[9]);

extern void tools_timeget_otc(char timestring[30]);

#endif /* COMMON_UTIL_H_ */
