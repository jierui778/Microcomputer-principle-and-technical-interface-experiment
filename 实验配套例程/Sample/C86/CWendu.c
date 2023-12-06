/**********************************************************
 * 文件名: Wendu.c
 * 功能描述: 温度闭环控制
 **********************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <conio.h>

#define M8255_A		0x600
#define M8255_B		0x602
#define M8255_C		0x604
#define M8255_CON	0x606

#define M8254_0		0x6c0
#define M8254_1		0x6c2
#define M8254_2		0x6c4
#define M8254_CON	0x6c6

#define AD0809		0x640

void  interrupt irq6h (void);
void  interrupt irq7h (void);

void init(void);
void pid(void);
void put_com(void);
void m_main(void);
void delay(int time);

int mmul(int x,int y);                /*16位乘法，溢出赋极值*/
int change32_16(int z,int t);         /*将32位数转化成16位*/
char change16_8(int wd);              /*将16位数转化成8位*/
int madd(int x,int y);                /*16位加法，溢出赋极值*/

int TS=0x64,X=0x80;
int SPEC=0x30,IBAND=0x60,KP=12,KI=18,KD=32;
int CH1,CH2,CH3,CK,TC,FPWM,CK_1,AAAA,VAA,BBB,VBB;
int TKMARK,ADMARK,ADVALUE;
int YK,EK_1,AEK,BEK,EK;

static int a[0x1ff] = {0x14,0x14,0x14,0x14,0x14,0x14,0x14,0x14,0x14,0x14,
		0x15,0x16,0x17,0x18,0x19,0x1a,0x1b,0x1c,0x1d,0x1e,0x1e,0x1f,0x20,0x21,
		0x22,0x23,0x24,0x25,0x26,0x27,0x28,0x29,0x2a,0x2b,0x2c,0x2d,0x2e,0x2f,
		0x31,0x32,0x32,0x33,0x34,0x35,0x36,0x37,0x38,0x39,0x3a,0x3b,0x3c,0x3d,
		0x3e,0x3f,0x40,0x42,0x43,0x44,0x45,0x46,0x47,0x48,0x49,0x4a,0x4b,0x4c,
		0x4d,0x4e,0x4f,0x50,0x4f,0x50,0x51,0x52,0x53,0x54,0x55,0x56,0x57,0x58,	
		0x59,0x5a,0x5b,0x5c,0x5d,0x5e,0x5f,0x60,0x61,0x62,0x63,0x64,0x64,0x65,
		0x65,0x66,0x66,0x67,0x68,0x69,0x6a,0x6b,0x6c,0x6d,0x6e,0x6e,0x6f,0x6f,	
		0x70,0x71,0x72,0x73,0x74,0x75,0x76,0x77,0x78,0x79,0x7a,0x7b,0x7c,0x7d,
		0x7e,0x7f,0x80,0x81,0x82,0x83,0x84,0x84,0x85,0x86,0x87,0x88,0x89,0x8a,
		0x8b,0x8c,0x8d,0x8e,0x8f,0x90,0x91,0x92,0x93,0x94,0x95,0x96,0x97,0x98,
		0x99,0x9a,0x9b,0x9b,0x9c,0x9c,0x9d,0x9d,0x9e,0x9e,0x9f,0x9f,0xa0,0xa1,
		0xa2,0xa3,0xa4,0xa5,0xa6,0xa7,0xa8,0xa9,0xaa,0xab,0xac,0xad,0xae,0xaf,
		0xb0,0xb0,0xb1,0xb2,0xb3,0xb4,0xb4,0xb5,0xb6,0xb7,0xb8,0xb9,0xba,0xbb,
		0xbd,0xbe,0xbe,0xc1,0xc2,0xc3,0xc4,0xc5,0xc6,0xc8,0xca,0xcc,0xce,0xcf,
		0xd0,0xd1,0xd2,0xd4,0xd5,0xd6,0xd7,0xd8,0xd9,0xda,0xdb,0xdc,0xdd,0xde,
		0xe3,0xe6,0xe9,0xec,0xf0,0xf2,0xf6,0xfa,0xff,0xff,0xff,0xff,0xff,0xff,
		0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
		0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff};

void main()
{
   init();
   _asm STI
   for(;;) m_main();
}

void delay(int time)
{
   int i;
   for(i=0;i<=time;i++);
   return;
}

void init(void)
{
	int m_ip;
	if(_CS==0) m_ip=0x2000;
	else m_ip=0x0000;
	_asm{
		push ds
		xor ax,ax
		mov ds,ax
		mov ax,offset irq6h
		add ax,m_ip
		mov si,0038h
		mov [si],ax
		mov ax,cs
		mov si,003ah
		mov [si],ax
		mov ax,offset irq7h
		add ax,m_ip
		mov si,003ch
		mov [si],ax
		mov ax,cs
		mov si,003eh
		mov [si],ax
		cli
		pop ds
		mov al,2fh
		out 21h,al
		}
	YK=EK=EK_1=AEK=BEK=CK=CK_1=BBB=VBB=ADVALUE=TC=ADMARK=TKMARK=0x00;
	FPWM=0x01;
	AAAA=VAA=0x7f;
	//
	outp(M8255_CON,0x90);
	outp(M8255_B,0x00);
	outp(M8254_CON,0x36);
	outp(M8254_0,0x10);
	outp(M8254_0,0x27);
	return;
}

void m_main(void)
{
	for(;;) if(TKMARK==0x01) break;
		TKMARK=0x00;

	for(;;) if(ADMARK==0x01) break;
		ADMARK=0x00;
	YK=a[ADVALUE];
	pid();
	if(CK <= 0X80)  AAAA=0x10;
	else            AAAA=CK-0x80;
	BBB=0x7f-AAAA;
	CH1=SPEC;
	CH2=YK;
	CH3=CK;
	put_com();

	return;
}

void put_com(void)
{
	char temp;
	temp = inp(0x03fb);
	temp = temp&0x7f;
	outp(0x03fb, temp);
	for(;;)  if ((inp(0x03fd)&0x20)!=0)	break;
	outp(0x03F8, CH1);

	for(;;)  if ((inp(0x03fd)&0x20)!=0)	break;
	outp(0x03F8, CH2);
	
	for(;;)  if ((inp(0x03fd)&0x20)!=0)	break;
	outp(0x03F8, CH3);

	return;
}

void pid(void)
{
	int K,P,I,D;
	char z,t;
	K=P=I=D=0;
	EK=SPEC-YK;
	BEK=EK-EK_1-AEK;
	AEK=EK-EK_1;

	if ( abs(EK)>abs(IBAND)) I=0;
	else I=(EK*TS)/KI;
	P=AEK;
	D=((KD/TS)*BEK)/10000;
	K=madd(I,P);
	K=madd(D,K);
	K=mmul(K,KP);
	K=K/10;
	CK=K+CK_1;

	CK=change16_8(CK);
	CK_1=CK;
	EK_1=EK;
	CK=CK+X;
}

void interrupt irq6h(void)
{
	outp(AD0809, 00);
	if(TC<TS) TC++;
	else
	{
		TKMARK=0x01;
		TC=0x00;
	}
	if(FPWM==0x01)
	{
		if(VAA!=0x00)
		{
			VAA=VAA-1;
			outp(M8255_B, 0x01);
		}
		else
		{
			FPWM=0x02;
			VBB=BBB/2;
		}
	}
	if(FPWM==0x02)
	{
		if(VBB!=0x00)
		{
			VBB=VBB-1;
			outp(M8255_B, 0x00);
		}
		else
		{
			FPWM=0x01;
			VAA=AAAA/2;
		}
	}
	outp(0x20,0x20);
	return;
}

void interrupt irq7h(void)
{
	ADVALUE=inp(AD0809);
	ADMARK=0x01;
	outp (0x20,0x20);
	return;
}

int mmul( int x,int y)
{
	int s,t,z;
	s=x*y;
	z=_AX;
	t=_DX;
	s=change32_16(z,t);
	return(s);
}

int change32_16(int z,int t)
{  
	int s;

	if(t==0)
	{
		if((z&0x8000)==0) s=z;
		else s=0x7fff;
	}
	else if ((t&0xffff)==0xffff)
	{
		if((z&0x8000)==0) s=0x8000;
		else s=z;
	}
	else if ((t&0x8000)==0) s=0x7fff;
	else s=0x8000;
	return(s);
}

int madd(int x,int y)
{
	int t;
	t=x+y;
	if(x>=0 && y>=0)
	{ if((t&0x8000)!=0) t=0x7fff; }
	else if (x<=0 && y<=0)
	{ if((t&0x8000)==0) t=0x8000; }
	return(t);
}

char change16_8(int wd)
{
	char z,t,s;

	_AX=wd;
	z=_AL;
	_AX=wd;
	t=_AH;

	if(t==0x00)
	{
		if((z&0x80)==0) s=z;
		else s=0x7f;
	}
	else if ((t&0xff)==0xff)
	{
		if((z&0x80)==0) s=0x80;
		else s=z;
	}
	else if((t&0x80)==0) s=0x7f;
	else s=0x80;
	return(s);
}