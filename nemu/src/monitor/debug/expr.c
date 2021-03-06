#include "nemu.h"
#include<stdlib.h>
/* We use the POSIX regex functions to process regular expressions.
 * Type 'man regex' for more information about POSIX regex functions.
 */
#include <sys/types.h>
#include <regex.h>

extern CPU_state cpu;
enum {
	NOTYPE = 256, EQ,ID,NUM,HEX,NOT,AND,OR,NEQ,LEQ,GEQ,LS,GT,MINUS,DEF,EAX=400,ECX,EDX,EBX,ESP,EBP,ESI,EDI,EIP

};

static struct rule {
	char *regex;
	int token_type;
} rules[] = {

	/* TODO: Add more rules.
	 * Pay attention to the precedence level of different rules.
	 */
	{"\\$eax",EAX},
	{"\\$ecx",ECX},
	{"\\$edx",EDX},
	{"\\$ebx",EBX},
	{"\\$esp",ESP},
	{"\\$ebp",EBP},
	{"\\$esi",ESI},
	{"\\$edi",EDI},
	{"\\$eip",EIP},
	{"<=",LEQ},
	{"!=",NEQ},
	{">=",GEQ},
	{"<",LS},
	{">",GT},
	{"!",NOT},					//not
	{"&&",AND},					//and
	{"\\|\\|",OR},					//or				
	{" +",	NOTYPE},				// spaces
	{"0x[A-Fa-f0-9]+",HEX},				//Hex
	{"[0-9]+",NUM},					//NUM
	{"\\(",'('},
	{"\\)",')'},
	{"\\/",'/'},					//divide
	{"\\*",'*'},					//multiply
	{"\\+", '+'},					// plus
	{"\\-",'-'},					//minus
	{"==", EQ}						// equal
};

#define NR_REGEX (sizeof(rules) / sizeof(rules[0]) )

static regex_t re[NR_REGEX];

/* Rules are used for many times.
 * Therefore we compile them only once before any usage.
 */
void init_regex() {
	int i;
	char error_msg[128];
	int ret;

	for(i = 0; i < NR_REGEX; i ++) {
		ret = regcomp(&re[i], rules[i].regex, REG_EXTENDED);
		if(ret != 0) {
			regerror(ret, &re[i], error_msg, 128);
			Assert(ret == 0, "regex compilation failed: %s\n%s", error_msg, rules[i].regex);
		}
	}
}

typedef struct token {
	int type;
	int precedent;
	char str[32];
} Token;

Token tokens[32];
int nr_token;
int pd=1;
static bool make_token(char *e) {
	int position = 0;
	int i;
	regmatch_t pmatch;
	
	nr_token = 0;

	while(e[position] != '\0') {
		/* Try all rules one by one. */
		for(i = 0; i < NR_REGEX; i ++) {
			if(regexec(&re[i], e + position, 1, &pmatch, 0) == 0 && pmatch.rm_so == 0) {
				char *substr_start = e + position;
				int substr_len = pmatch.rm_eo;

				Log("match rules[%d] = \"%s\" at position %d with len %d: %.*s", i, rules[i].regex, position, substr_len, substr_len, substr_start);
				position += substr_len;

				/* TODO: Now a new token is recognized with rules[i]. Add codes
				 * to record the token in the array `tokens'. For certain types
				 * of tokens, some extra actions should be performed.
				 */
				int index;
				switch(rules[i].token_type) {
					case EAX:
					case ECX:
					case EDX:
					case EBX:
					case ESP:
					case EBP:
					case ESI:
					case EDI:
					case EIP:
						index=rules[i].token_type%400;
						unsigned int* p=(unsigned int*)&cpu;
						snprintf(tokens[nr_token].str,32,"%d",*(p+index));
						tokens[nr_token].type=NUM;
						nr_token++;
						break;	
					case OR:
						tokens[nr_token].precedent=pd;
						tokens[nr_token].type=rules[i].token_type;
						nr_token++;
						break;
					case AND:
						tokens[nr_token].precedent=++pd;
						tokens[nr_token].type=rules[i].token_type;
						nr_token++;
						break;
					case EQ:
					case NEQ:
						tokens[nr_token].precedent=++pd;
						tokens[nr_token].type=rules[i].token_type;
						nr_token++;
						break;
					case GEQ:
					case LEQ:
					case LS:
					case GT:
						tokens[nr_token].precedent=++pd;
						tokens[nr_token].type=rules[i].token_type;
						nr_token++;
						break;
				 	case '+':
					case '-':
						tokens[nr_token].precedent=++pd;
						tokens[nr_token].type=rules[i].token_type;
						nr_token++;
						break;
					case '*':
					case '/':
						tokens[nr_token].precedent=++pd;
						tokens[nr_token].type=rules[i].token_type;
						nr_token++;
						break;
					case DEF:
					case NOT:
						tokens[nr_token].precedent=++pd;
						tokens[nr_token].type=rules[i].token_type;
						nr_token++;
						break;
					case '(':
					case ')':
						tokens[nr_token].type=rules[i].token_type;
						nr_token++;	
						break;
					case NOTYPE:
						break;
					case NUM:
						strncpy(tokens[nr_token].str,e+position-substr_len,substr_len>31?31:substr_len); 
						tokens[nr_token].str[31]='\0'; 
						tokens[nr_token].type=rules[i].token_type; nr_token++;
						break;				
					case HEX:
						strncpy(tokens[nr_token].str,e+position-substr_len,substr_len>31?31:substr_len); 
						tokens[nr_token].str[31]='\0'; 
						int temp;
						sscanf(tokens[nr_token].str,"0x%x",&temp);
						snprintf(tokens[nr_token].str,32,"%d",temp);	
						tokens[nr_token].type=NUM; nr_token++;
						break;	
					default: panic("please implement me");
				}

				break;
			}
		}

		if(i == NR_REGEX) {
			printf("no match at position %d\n%s\n%*.s^\n", position, e, position, "");
			return false;
		}
	}

	return true; 
}

int stackeval[32],stackopr[32];
int stackeval_t=0,stackopr_t=0;

int eval(){
	stackeval_t=stackopr_t=0;
	int i=0;
	for(i=0;i!=nr_token;i++){
		if(tokens[i].type==NUM){
			stackeval[stackeval_t++]=i;
		}else if(tokens[i].type=='('){
			stackopr[stackopr_t++]=i;
		}else if(tokens[i].type==')'){
			stackopr_t--;
			while(tokens[stackopr[stackopr_t]].type!='('){
				stackeval[stackeval_t++]=stackopr[stackopr_t];
				stackopr_t--;
			}
		}else{
		if(stackopr_t==0)stackopr[stackopr_t++]=i;
	        else if(tokens[i].precedent>=tokens[stackopr[stackopr_t-1]].precedent)stackopr[stackopr_t++]=i;
		else {
				while(stackopr_t>0&&tokens[stackopr[stackopr_t-1]].precedent>tokens[i].precedent)
					{
						stackopr_t--;
						stackeval[stackeval_t++]=stackopr[stackopr_t];
					}
				stackopr[stackopr_t++]=i;
		}
		}
	}
	while(stackopr_t>0){
		stackeval[stackeval_t++]=stackopr[--stackopr_t];
	}
	int result[32];
	int resindex=0;
	for(i=0;i!=stackeval_t;i++)
	{
		if(tokens[stackeval[i]].type==NUM)result[resindex++]=atoi(tokens[stackeval[i]].str);
		else{
			int t=tokens[stackeval[i]].type;
			if(t==DEF||t==NOT||t==MINUS){
				if(resindex<1)assert(0);
				int a=result[resindex-1];
				switch(t){
					case DEF:
						a=hwaddr_read(a,4);
						break;
					case MINUS:
						a=-a;
						break;
					case NOT:
						a=!a;
						break;			
				}
				result[resindex-1]=a;
			}
			else{
			        if(resindex<2)assert(0);
				int a=result[resindex-2];
				int b=result[resindex-1];
				switch(tokens[stackeval[i]].type){
					case GEQ:a=(a>=b);break;
					case LEQ:a=(a<=b);break;
					case LS: a=(a<b);break;
					case GT: a=(a>b);break;
					case EQ: a=(a==b);break;
					case NEQ:a=(a!=b);break;
					case AND:a=(a&&b);break;
					case OR: a=(a||b);break;
					case '+':a+=b;break;
					case '-':a-=b;break;
					case '*':a*=b;break;
					case '/':
						if(b==0)assert(0);
						a/=b;
						break;
					default:break;
				}
				result[resindex-2]=a;
				resindex--;
			}
		}
	}	
	return result[0];
}
uint32_t expr(char *e, bool *success) {
	if(!make_token(e)) {
		*success = false;
		return 0;
	}
	int i=0;
	for(i=0;i!=nr_token;i++){
		int type=tokens[i].type;
		if(type=='*'&&(i==0||tokens[i-1].type!=NUM))tokens[i].type=DEF;
		else if(type=='-'&&(i==0||tokens[i-1].type!=NUM))tokens[i].type=MINUS;
	}
	int le=0;
	for(i=0;i!=nr_token;i++){
		if(le<0)assert(0);
		if(tokens[i].type=='(')le++;
		else if(tokens[i].type==')')le--;
	}
	if(le!=0)assert(0);
	/* TODO: Insert codes to evaluate the expression. */
	return eval();
} 
