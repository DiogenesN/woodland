// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef GETINTVALUEFROMCONF_H_
#define GETINTVALUEFROMCONF_H_

int get_int_value_from_conf(char *fullPathToConf, char *nameToGetValueFrom);
double get_double_value_from_conf(char *fullPathToConf, char *nameToGetValueFrom);
char *get_char_value_from_conf(char *fullPathToConf, char *nameToGetValueFrom);

#endif
