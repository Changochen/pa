#include "monitor/watchpoint.h"
#include "monitor/expr.h"

#define NR_WP 32

static WP wp_pool[NR_WP];
static WP *head, *free_;

void init_wp_pool() {
	int i;
	for(i = 0; i < NR_WP; i ++) {
		wp_pool[i].NO = i;
		wp_pool[i].next = &wp_pool[i + 1];
	}
	wp_pool[NR_WP - 1].next = NULL;

	head = NULL;
	free_ = wp_pool;
}

/* TODO: Implement the functionality of watchpoint */
WP* new_wp(){
	if(free_==NULL)assert(0);
	else{
		WP* new_p=free_;
		free_=free_->next;
		new_p->next=head;
		head=new_p;
		return new_p;
	}
}

void free_wp(WP *wp){
	if(wp==NULL)return;
	else{
		if(head==wp){
			head=head->next;
			wp->next=free_;
			free_=wp;
			return;
		}
		WP *temp=head;
		while(temp->next!=NULL&&temp->next!=wp)temp=temp->next;
		if(temp->next==NULL)assert(0);
		WP *freetemp=temp->next;
		temp->next=temp->next->next;	
		freetemp->next=free_;
		free_=freetemp;
	}
}
