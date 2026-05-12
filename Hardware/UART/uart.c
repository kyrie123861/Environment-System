// uart.c - USART1 串口驱动 (PM2.5传感器)
// 
// PA9=TX(复用推挽), PA10=RX(浮空输入), 9600-8-N-1
// 
// 波特率: BRR = 72MHz / (16×9600) = 468.75
// 整数468(0x1D4), 小数0.75×16=12(0xC) -> BRR = 0x1D4C
// 
// 环形缓冲区: 64字节, 中断写入(head), 主程序读取(tail)
// head==tail: 空, (head+1)%SIZE==tail: 满(留一个空位)

#include "uart.h"
#include "main.h"

// 环形接收缓冲区 (64字节, 可缓冲2帧PMS5003数据)
static volatile uint8_t rx_buf[PM25_RX_BUF_SIZE];
static volatile uint8_t rx_head = 0;  // ISR写入位置
static volatile uint8_t rx_tail = 0;  // 主程序读取位置

void UART_Init(void)
{
// 1. 开启GPIOA和USART1时钟 (均在APB2)
	RCC->APB2ENR |= RCC_APB2ENR_IOPAEN;
	RCC->APB2ENR |= RCC_APB2ENR_USART1EN;

// 2. PA9(TX): 复用推挽输出50MHz (CNF=10复用推挽, MODE=11 -> 0xB)
	GPIOA->CRH |= GPIO_CRH_MODE9;
	GPIOA->CRH |= GPIO_CRH_CNF9_1;
	GPIOA->CRH &= ~GPIO_CRH_CNF9_0;

// 3. PA10(RX): 浮空输入 (CNF=01浮空, MODE=00输入)
// 浮空输入: 引脚为高阻态, 电平完全由外部(传感器TX)决定
	GPIOA->CRH &= ~GPIO_CRH_MODE10;
	GPIOA->CRH &= ~GPIO_CRH_CNF10_1;
	GPIOA->CRH |= GPIO_CRH_CNF10_0;

// 4. 波特率9600: BRR = 0x1D4C
	USART1->BRR = 0x1D4C;

// 5. 使能USART + TX + RX + RX中断
// UE(bit13)=使能, TE(bit3)=发送, RE(bit2)=接收, RXNEIE(bit5)=接收中断
	USART1->CR1 |= (USART_CR1_UE | USART_CR1_TE | USART_CR1_RE | USART_CR1_RXNEIE);

// 6. NVIC: 抢占优先级=1(中等), 通道号37=USART1
	NVIC_SetPriorityGrouping(3);
	NVIC_SetPriority(USART1_IRQn, 1);
	NVIC_EnableIRQ(USART1_IRQn);
}

// 发送一个字节(阻塞): 等TXE=1(发送寄存器空), 写DR
void UART_SendChar(uint8_t ch)
{
	while ((USART1->SR & USART_SR_TXE) == 0)
	{}
	USART1->DR = ch;
}

// 接收一个字节(阻塞): 等RXNE=1(收到数据), 读DR
uint8_t UART_ReceiveChar(void)
{
	while ((USART1->SR & USART_SR_RXNE) == 0)
	{}
	return (uint8_t)(USART1->DR & 0xFF);
}

// 查询环形缓冲区可读字节数
uint8_t UART_Available(void)
{
	if (rx_head >= rx_tail)
		return rx_head - rx_tail;
	else
		return PM25_RX_BUF_SIZE + rx_head - rx_tail;  // 回绕情况
}

// 从环形缓冲区读一个字节(非阻塞): 返回1=成功, 0=空
uint8_t UART_ReadByte(uint8_t *data)
{
	if (rx_head == rx_tail)
		return 0;  // 缓冲区空

	*data = rx_buf[rx_tail];
	rx_tail = (rx_tail + 1) % PM25_RX_BUF_SIZE;
	return 1;
}

// 清空环形缓冲区: 头尾指针归零
void UART_Flush(void)
{
	rx_head = 0;
	rx_tail = 0;
}

// USART1中断服务: 每收到1字节触发
// 仅做快速数据搬移(从DR到环形缓冲), 帧解析留给主循环
void USART1_IRQHandler(void)
{
	uint8_t next_head;

// 处理接收中断 (RXNE=1)
	if (USART1->SR & USART_SR_RXNE)
	{
		uint8_t data = (uint8_t)(USART1->DR & 0xFF);  // 读DR自动清零RXNE

		next_head = (rx_head + 1) % PM25_RX_BUF_SIZE;

// 缓冲区未满才写入, 满了丢弃 (保护已缓冲的有效帧)
		if (next_head != rx_tail)
		{
			rx_buf[rx_head] = data;
			rx_head = next_head;
		}
	}

// 处理溢出错误: 收到新数据但旧数据未被读走
// 读DR再读SR可清除ORE标志
	if (USART1->SR & USART_SR_ORE)
	{
		(void)USART1->DR;
		(void)USART1->SR;
	}
}

// 重写fputc: 让printf输出重定向到USART1串口
int fputc(int ch, FILE *file)
{
	(void)file;  // 兼容ARMCLANG: 消除unused parameter警告
	UART_SendChar((uint8_t)ch);
	return ch;
}
