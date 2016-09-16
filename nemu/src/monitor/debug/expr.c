#include "nemu.h"
#include<stdlib.h>
/* We use the POSIX regex functions to process regular expressions.
 * Type 'man regex' for more information about POSIX regex functions.
 */
#include <sys/types.h>
#include <regex.h>

#define MAX_TOKEN 32;
enum {
	NOTYPE = 256, EQ,ID,NUM,HEX,

};

static struct rule {
	char *regex;
	int token_type;
} rules[] = {

	/* TODO: Add more rules.
	 * Pay attention to the precedence level of different rules.
	 */

	{" +",	NOTYPE},				// spaces
	{"[0-9]+",NUM},					//NUM
	{"\\(",'('},
	{"\\)",')'},
	{"0x[A-Fa-f0-9]+",HEX},				//Hex
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

				switch(rules[i].token_type) {
				 	case '+':
					case '-':
						tokens[nr_token].precedent=1;
						tokens[nr_token].type=rules[i].token_type;
						nr_token++;
						break;
					case '*':
					case '/':
						tokens[nr_token].precedent=2;
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
			stackopr_t++;
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
	for(i=0;i!=stackeval_t;i++)
	{
		printf("%c",tokens[stackeval[i]].type==NUM?'N':tokens[stackeval[i]].type);
	}
	printf("\n");
	int result[32];
	int resindex=0;
	for(i=0;i!=stackeval_t;i++)
	{
		if(tokens[stackeval[i]].type==NUM)result[resindex++]=stackeval[i];
		else{
			if(resindex<2)assert(0);
			else{
				int a=atoi(tokens[result[resindex-1]].str);
				int b=atoi(tokens[result[resindex-2]].str);
				printf("%d %d\n",a,b);
				switch(tokens[stackeval[i]].type){
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
	int le=0;
	for(;i!=nr_token;i++){
		if(le<0)assert(0);
		if(tokens[i].type=='(')le++;
		else if(tokens[i].type==')')le--;
	}
	if(le!=0)assert(0);
	/* TODO: Insert codes to evaluate the expression. */
	return eval();
} 
