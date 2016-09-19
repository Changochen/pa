#include "monitor/monitor.h"
#include "monitor/expr.h"
#include "monitor/watchpoint.h"
#include "nemu.h"
#include <stdlib.h>
#include <readline/readline.h>
#include <readline/history.h>
extern WP* gethead();
extern WP* new_wp();
union ad{
	char *addr_p;
	uint32_t addr_u;
};
void cpu_exec(uint32_t);
static int cmd_d(char *arg)
{
	return 0;
}
static int cmd_si(char *arg)
{
		
	uint32_t n;
	if(arg!=NULL)n=atoi(arg);
	else n=1;
	cpu_exec(n);
	return 0;
}
extern CPU_state cpu;
/* We use the `readline' library to provide more flexibility to read from stdin. */
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
static int cmd_info(char *args){
	if(*args=='r'){
		printf("EAX:  0x%x\t%d\n",cpu.gpr[0]._32,cpu.gpr[0]._32);
		printf("ECX:  0x%x\t%d\n",cpu.gpr[1]._32,cpu.gpr[1]._32);
		printf("EDX:  0x%x\t%d\n",cpu.gpr[2]._32,cpu.gpr[2]._32);
		printf("EBX:  0x%x\t%d\n",cpu.gpr[3]._32,cpu.gpr[3]._32);
		printf("ESP:  0x%x\n",cpu.gpr[4]._32);
		printf("EBP:  0x%x\n",cpu.gpr[5]._32);
		printf("ESI:  0x%x\n",cpu.gpr[6]._32);
		printf("EDI:  0x%x\n",cpu.gpr[7]._32);
		printf("EIP:  0x%x\n",cpu.eip);
	}else if(*args=='w'){
		WP* head=gethead();
		while(head!=NULL){
			if(head->bp==false)printf("Watchpoint %d:%s\n",head->NO,head->expr);
			head=head->next;
		}
	}
	return 0;
}

static int cmd_p(char *args)
{
	bool f=true;
	unsigned int res=expr(args,&f);
	if(f==false)assert(0);
	else printf("%d\n",res);
	return 0;
}
static int cmd_w(char *args)
{
	bool f=true;
	unsigned int temp=expr(args,&f);
	if(f==false)assert(0);
	WP *new_p=new_wp();
	if(strncmp(args,"$eip==",6)==0)new_p->bp=true;
	else new_p->bp=false;
	strncpy(new_p->expr,args,1024);
	new_p->old_value=temp;
	if(new_p->bp==false)printf("Watchpoint %d set at %s\n",new_p->NO,args);
	else printf("Breakpoint set at 0x%x\n",(unsigned)strtol(args+6,NULL,16));
	return 0;
}
	
static int cmd_br(char *args){
	char p[1024];
	strncpy(p,"$eip==",1024);
	strncpy(p+6,args,1000);
	cmd_w(p);
	return 0;
}
static int cmd_x(char *args)
{
	char* arg1=strtok(args," ");
	char* arg2=strtok(NULL," ");
	if(arg2==NULL ||arg1==NULL)printf("Wrong usage!Type help for help\n");
	int k=atoi(arg1);
	unsigned int addr;
	addr=strtol(arg2,NULL,16);
	int i=0;
	for(i=0;i!=k;i++){
		if(i%4==0)printf("0x%08x: ",addr);
		printf("%08x ",hwaddr_read(addr,4));
		if((i+1)%4==0)printf("\n");
		addr+=4;
	}
	printf("\n");
	return 0;
}
static int cmd_q(char *args) {
	return -1;
}

static int cmd_help(char *args);

static struct {
	char *name;
	char *description;
	int (*handler) (char *);
} cmd_table [] = {
	{ "help", "Display informations about all supported commands", cmd_help },
	{ "c", "Continue the execution of the program", cmd_c },
	{ "q", "Exit NEMU", cmd_q },
	{"si","Excute n instructions.Usage: si [n]",cmd_si},
	{"info","Print the state of the running program",cmd_info},
	{"x","Scanf the address. Usage x [n]",cmd_x},
	{"p","Evaluate expression and print the value.Usage p expr",cmd_p},	
	{"w","Set watchpoint at expr.Usage p expr",cmd_w},
	{"br","Breakpoint set at address N.Usage br N",cmd_br},
	{"d","Delete breakpoint.Usage d N.",cmd_d},
	/* TODO: Add more commands */

};

#define NR_CMD (sizeof(cmd_table) / sizeof(cmd_table[0]))

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

void ui_mainloop() {
	while(1) {
		char *str = rl_gets();
		char *str_end = str + strlen(str);

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
