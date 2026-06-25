#ifndef MONITORING_DATA_H
#define MONITORING_DATA_H

int add_critical_sn(int);
int remove_critical_sn(int);
int add_critical_euid(char*);
int remove_critical_euid(char*);
int add_critical_pn(char*);
int remove_critical_pn(char*);
int is_critical(int, char*, char*);

#endif