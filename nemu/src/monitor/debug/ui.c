#include "monitor/monitor.h"
#include "monitor/expr.h"
#include "monitor/watchpoint.h"
#include "nemu.h"
#include "cpu/reg.h"

#include <stdlib.h>
#include <readline/readline.h>
#include <readline/history.h>

static void print_b(unsigned char a) //不懂有什么作用
{
	const static char hex_map[][2] = {
		"0", "1", "2", "3", "4", "5", "6", "7", 
		"8", "9", "a", "b", "c", "d", "e", "f",
	};
	printf("%s%s", hex_map[a>>4], hex_map[a&0xf]);
}

static void print_l(unsigned a)
{
	print_b((a>>24) & 0xff);
	print_b((a>>16) & 0xff);
	print_b((a>>8) & 0xff);
	print_b(a & 0xff);
}

void cpu_exec(uint32_t);

/* We use the ``readline'' library to provide more flexibility to read from stdin. */
char* rl_gets() {
	static char *line_read = NULL;

	if (line_read) {
		free(line_read);
		line_read = NULL;
	}

	line_read = readline("(nemu) ");

	if (line_read && *line_read) {
		add_history(line_read);
	}

	return line_read;
}

static int cmd_c(char *args) {
	cpu_exec(-1);
	return 0;
}

static int cmd_q(char *args) {
	return -1;
}

static int cmd_help(char *args);

static int cmd_si(char *args);

static int cmd_info(char *args);

static int cmd_x(char *args);

static struct {
	char *name;
	char *description;
	int (*handler) (char *);
} cmd_table [] = {
	{ "help", "Display informations about all supported commands", cmd_help },
	{ "c", "Continue the execution of the program", cmd_c },
	{ "q", "Exit NEMU", cmd_q },
	{ "si", "Step over n times and pause. If n is not given, n = 1", cmd_si},
	{ "info", "Info [r] print the values of all of the registers; Info [n] print the imformation of watchpoint", cmd_info},
	{ "x", "get the value of expr, use the result as the begin adress of memory, output the successly n 4 bytes in the form of 0x", cmd_x},
	/* TODO: Add more commands */

};
//fuck
#define NR_CMD (sizeof(cmd_table) / sizeof(cmd_table[0])) //cmd指令的数量

static int cmd_help(char *args) {
	/* extract the first argument */
	char *arg = strtok(NULL, " ");
	int i;

	if(arg == NULL) {
		/* no argument given */
		for(i = 0; i < NR_CMD; i ++) {
			printf("%s - %s\n", cmd_table[i].name, cmd_table[i].description);
		}
	}
	else {
		for(i = 0; i < NR_CMD; i ++) {
			if(strcmp(arg, cmd_table[i].name) == 0) {
				printf("%s - %s\n", cmd_table[i].name, cmd_table[i].description);
				return 0;
			}
		}
		printf("Unknown command '%s'\n", arg);
	}
	return 0;
}

static int cmd_si(char *args) {
	unsigned step_num;
	if (args == NULL) {
		cpu_exec(1);
		return 0;
	} else {
		if (sscanf(args, "%u", &step_num) > 0) {
			cpu_exec(step_num);
		} else {
			printf("Error, si must be followed by a number\n");
		}
	}
	return 0;
}	

static int cmd_info(char *args) {
	char type;
	if (args == NULL) {
		printf("Error, [info] must be followed by [b] or [r]!\n");
		return 0;
	}
	if (sscanf(args, "%c", &type) > 0) {
		if (type == 'r') {
			printf("eax=%d\n", cpu.eax);
			printf("ecx=%d\n", cpu.ecx);
			printf("edx=%d\n", cpu.edx);
			printf("ebx=%d\n", cpu.ebx);
			printf("esp=%d\n", cpu.esp);
			printf("ebp=%d\n", cpu.ebp);
			printf("esi=%d\n", cpu.esi);
			printf("edi=%d\n", cpu.edi);
			char _16_[8][5] = {
				"R_AX", "R_CX", "R_DX", "R_BX", "R_SP", "R_BP", "R_SI", "R_DI",
			};
			int i;
			for (i = R_EAX; i <= R_EDI; i++) {
				printf("%s=%d\n", _16_[i], cpu.gpr[i]._16);	
			}
			char _8_[8][5] = {
				"R_AL", "R_CL", "R_DL", "R_BL", "R_AH", "R_CH", "R_DH", "R_BH",
			};
			for (i = R_EAX; i <= R_EBX; i++) {
				printf("%s=%d\n", _8_[i], cpu.gpr[i]._8[0]);
				printf("%s=%d\n", _8_[i + 4], cpu.gpr[i]._8[1]);
			}
		} else if (type == 'b') {
			printf("Sorry, this function is developing!\n");
		}
	} else {
		printf("Error, [info] must be followed by [b] or [r]!\n");
	}
	return 0;
}

static int cmd_x(char *args)
{
	if (args == NULL) {
		printf("sorry, please input as the form x [number] [expression]\n");
		return 1;
	}
	unsigned num, start;
	char str[3];
	if (sscanf(args, "%u%2s%x", &num, str, &start) == 3) {
		if (strcmp(str, "0x") == 0 || strcmp(str, "0X") == 0) {
			swaddr_t begin = start; //这个类型到底什么鬼意思啊！！！
			unsigned i;
			for (i = 0; i < num; i++) {
				printf("%x:      ", begin);
				unsigned ret = swaddr_read(begin, 4); 
				print_l(ret);//将内存地址读成数据，再打印成16进制？
				begin += 4;
				printf(" \n");
			}
			return 0;
		}
	}
	printf("sorry, please input as the form x [number] [expression]\n");
	return 1;
}

void ui_mainloop() {
	while(1) {
		char *str = rl_gets();
		char *str_end = str + strlen(str); //读入字符串，用两个指针分别指向头和尾

		/* extract the first token as the command */
		char *cmd = strtok(str, " ");
		if(cmd == NULL) { continue; }

		/* treat the remaining string as the arguments,
		 * which may need further parsing
		 */
		char *args = cmd + strlen(cmd) + 1;
		if(args >= str_end) {
			args = NULL;
		}

#ifdef HAS_DEVICE
		extern void sdl_clear_event_queue(void);
		sdl_clear_event_queue();
#endif

		int i;
		for(i = 0; i < NR_CMD; i ++) {
			if(strcmp(cmd, cmd_table[i].name) == 0) {
				if(cmd_table[i].handler(args) < 0) { return; }
				break;
			}
		}

		if(i == NR_CMD) { printf("Unknown command '%s'\n", cmd); }
	}
}
