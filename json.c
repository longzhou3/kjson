/***************************************************************************
*            json.c
*
*  Tue January 05 01:23:48 2016
*  Copyright  2016  lk
*  <user@host>
****************************************************************************/
/*
* json.c
*
* Copyright (C) 2016 - lk
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 2 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program. If not, see <http://www.gnu.org/licenses/>.
*/

#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <ctype.h>
#include <stdlib.h>

#include "json.h"
#include "utils.h"

#define BLOCK_SIZE 1024 * 16

json_word_t* word_next(json_document_t* json_doc)
{
	json_doc->cur_word = json_doc->lookup;

	char look_char = *(json_doc->cur_pos + 1);

	while (isspace(look_char))
	{
		++json_doc->cur_pos;
		look_char = *(json_doc->cur_pos + 1);
	}

	if (look_char == '{'
		|| look_char == '}'
		|| look_char == '['
		|| look_char == ']'
		|| look_char == ':'
		|| look_char == ',')
	{
		json_doc->lookup.wtype = (int)look_char;
		json_doc->cur_pos++;
		json_doc->lookup.val_len = 1;
	}
	else if (look_char == '\"'
		|| look_char == '\'')
	{
		json_doc->lookup.wtype = WORD_STRING;
		json_doc->cur_pos += 2;
		json_doc->lookup.string_val = json_doc->cur_pos;
		while (*(json_doc->cur_pos) != look_char)
		{
			if (*(json_doc->cur_pos) == '\\')
			{
				++json_doc->cur_pos;
			}
			++json_doc->cur_pos;
		}
		json_doc->lookup.val_len = json_doc->cur_pos - json_doc->lookup.string_val;
	}
	else if (isdigit(look_char)
		|| look_char == '-'
		|| look_char == '+') {//TODO
		json_doc->lookup.wtype = WORD_INT;
		const char* pPos = json_doc->cur_pos + 1;

		json_doc->lookup.val_len = 0;
		while (isdigit(json_doc->cur_pos[1])
			|| json_doc->cur_pos[1] == '.'
			|| json_doc->cur_pos[1] == '-'
			|| json_doc->cur_pos[1] == '+')
		{
			if (json_doc->cur_pos[1] == '.')
			{
				json_doc->lookup.wtype = WORD_DOUBLE;
			}
			++json_doc->cur_pos;
			++json_doc->lookup.val_len;
		}
		if (json_doc->lookup.wtype == WORD_INT)
		{
			json_doc->lookup.int_val = atoi(pPos);
		}
		else
		{
			json_doc->lookup.double_val = atof(pPos);
		}

	}
	else if (isalpha(look_char) || look_char == '_') {//TODO
		json_doc->lookup.wtype = WORD_ID;
		json_doc->lookup.string_val = json_doc->cur_pos + 1;
		json_doc->lookup.val_len = 0;
		while (isalnum(json_doc->cur_pos[1])
			|| json_doc->cur_pos[1] == '_')
		{
			++json_doc->cur_pos;
			++json_doc->lookup.val_len;
		}

	}
	else {
		json_doc->lookup.wtype = -1;
		json_doc->cur_pos++;

	}

	return (json_word_t*)&(json_doc->cur_word);
}

json_word_t* word_lookup(json_document_t* json_doc)
{
	return (json_word_t*)&(json_doc->lookup);
}

void word_print(json_word_t* pword)
{
	switch (pword->wtype)
	{
	case WORD_ID:
		printf("key->%s,len=%d\n", pword->string_val, pword->val_len);
		break;
	case WORD_STRING:
		printf("string->%s,len=%d\n", pword->string_val, pword->val_len);
		break;
	case WORD_INT:
		printf("int->%d,len=%d\n", pword->int_val, pword->val_len);
		break;
	default:
		printf("char->%c\n", pword->wtype);
		break;
	}
	return;
}

static int json_init_document(json_document_t* json_doc, const char* file_path, FILE* file,
	const char* string_json, print_json_callback print_back)
{
	json_doc->file_path = file_path;
	json_doc->file = file;
	json_doc->json = string_json;
	json_doc->cur_pos = json_doc->json - 1;
	json_doc->json_len = strlen(json_doc->json);
	json_doc->cur_word.wtype = -1;
	json_doc->lookup.wtype = -1;
	json_doc->json_error.json_type = JSON_ERROR;
	json_doc->memery_pool = NULL;
	json_doc->print_callback = print_back;
	word_next(json_doc);
	return K_SUCCESS;
}

static int json_reset_document(json_document_t* json_doc)
{
	json_doc->file_path = NULL;
	json_doc->file = NULL;
	json_doc->json = NULL;
	json_doc->cur_pos = NULL;
	json_doc->json_len = 0;
	json_doc->cur_word.wtype = -1;
	json_doc->lookup.wtype = -1;
	json_doc->json_error.json_type = JSON_ERROR;
	json_doc->memery_pool = NULL;
	json_doc->double_quote = K_TRUE;
	json_doc->quote_key = K_TRUE;
	json_doc->print_callback = NULL;
	json_doc->malloc_fast = K_TRUE;
	return K_SUCCESS;
}

int json_init_document_empty(json_document_t* json_doc, print_json_callback print_back)
{
	json_reset_document(json_doc);
	pool_t* pool = pool_create("json_pool", BLOCK_SIZE, BLOCK_SIZE);
	json_doc->memery_pool = pool;
	return K_SUCCESS;
}

int json_init_document_file(json_document_t* json_doc, const char* file_path, print_json_callback print_back)
{
	char* string_json;
	FILE* file;
	int   file_len;
	file = fopen(file_path, "rb");
	if (file == NULL)
	{
		LOG("open file failed->%s\n", file_path);
		json_reset_document(json_doc);
		return K_ERROR;
	}

	fseek(file, 0, SEEK_END);
	file_len = ftell(file);
	LOG("json file len->%d\n", file_len);
	fseek(file, 0, SEEK_SET);
	string_json = (char *)malloc(file_len * sizeof(char) + 1);
	fread(string_json, file_len, sizeof(char), file);
	string_json[file_len] = '\0';

	return json_init_document(json_doc, file_path, file, string_json, print_back);
}

int json_init_document_string(json_document_t* json_doc, const char* string_json, print_json_callback print_back)
{
	return json_init_document(json_doc, NULL, NULL, string_json, print_back);
}

void json_set_dobule_quote(json_document_t* json_doc, bool_t bdquote)
{
	json_doc->double_quote = bdquote;
}

void json_set_quote_key(json_document_t* json_doc, bool_t bquote_key)
{
	json_doc->quote_key = bquote_key;
}

int json_destory_document(json_document_t* json_doc)
{
	if (json_doc->file != NULL)
	{
		fclose(json_doc->file);
		free(json_doc->json);
	}

	if (json_doc->memery_pool != NULL)
	{
		pool_destory(json_doc->memery_pool);
	}

	json_reset_document(json_doc);
	return K_SUCCESS;
}

json_value_t* json_new_value(json_type_t json_type, pool_t* pool)
{
	json_value_t* json_val = (json_value_t*)pool_malloc_fast(pool, sizeof(json_value_t));
	list_init((list_t*)json_val);
	json_val->json_type = json_type;
	if (json_type == JSON_OBJECT
		|| json_type == JSON_ARRAY)
	{
		list_init((list_t*)&(json_val->list_val));
	}
	return json_val;
}

json_value_t* json_set_int_word(json_value_t* json_val, json_word_t* json_word)
{
	assert(json_val->json_type == JSON_INT);
	assert(json_word->wtype == WORD_INT);
	json_val->int_val = json_word->int_val;
	return json_val;
}

json_value_t* json_set_double_word(json_value_t* json_val, json_word_t* json_word)
{
	assert(json_val->json_type == JSON_DOUBLE);
	assert(json_word->wtype == WORD_DOUBLE);
	json_val->double_val = json_word->double_val;
	return json_val;
}

json_value_t* json_set_id_word(json_value_t* json_val, json_word_t* json_word, pool_t* pool)
{
	assert(json_val->json_type == JSON_ID);
	assert(json_word->wtype == WORD_ID
		|| json_word->wtype == WORD_STRING);
	if (json_word->wtype == WORD_STRING)
	{
		return json_set_string_word(json_val, json_word, pool);
	}

	if (strncmp("true", json_word->string_val, json_word->val_len) == 0)
	{
		json_val->json_type = JSON_BOOL;
		json_val->bool_val = K_TRUE;
	}
	else if (strncmp("false", json_word->string_val, json_word->val_len) == 0)
	{
		json_val->json_type = JSON_BOOL;
		json_val->bool_val = K_FALSE;
	}
	else if (strncmp("null", json_word->string_val, json_word->val_len) == 0)
	{
		json_val->json_type = JSON_NULL;
		json_val->int_val = 0;
	}
	else
	{
		json_val->string_val = (char*)pool_malloc(pool, json_word->val_len + 1);
		memcpy(json_val->string_val, json_word->string_val, json_word->val_len);
		(json_val->string_val)[json_word->val_len] = '\0';
	}
	return json_val;
}

json_value_t* json_set_string_word(json_value_t* json_val, json_word_t* json_word, pool_t* pool)
{
	assert(json_val->json_type == JSON_STRING
		|| json_val->json_type == JSON_ID);
	assert(json_word->wtype == WORD_STRING);
	json_val->string_val = (char*)pool_malloc(pool, json_word->val_len + 1);
	memcpy(json_val->string_val, json_word->string_val, json_word->val_len);
	(json_val->string_val)[json_word->val_len] = '\0';
	return json_val;
}

json_value_t* json_set_pair(json_value_t* json_pair, json_value_t* json_key, json_value_t* json_val)
{
	assert(json_pair->json_type == JSON_PAIR);
	json_pair->pair_val.json_key = json_key;
	json_pair->pair_val.json_val = json_val;
	return json_pair;
}
json_value_t* json_list_push(json_value_t* json_list, json_value_t* json_val)
{
	assert(json_list->json_type == JSON_OBJECT
		|| json_list->json_type == JSON_ARRAY);
	list_insert_before((list_t*)&(json_list->list_val), (list_t*)json_val);
	return json_list;
}

json_value_t* json_parse_pair(json_document_t* json_doc, pool_t* pool)
{
	json_value_t* json_pair = json_new_value(JSON_PAIR, pool);
	json_value_t* json_key = json_parse_id(json_doc, pool);
	json_word_t* plook = word_next(json_doc);
	assert(plook->wtype == ':');
	json_value_t* json_val = json_parse_value(json_doc, pool);
	json_set_pair(json_pair, json_key, json_val);
	plook = word_lookup(json_doc);
	assert(plook->wtype == ','
		|| plook->wtype == '}');
	return json_pair;
}

json_value_t* json_parse_object(json_document_t* json_doc, pool_t* pool)
{
	json_value_t* json_list = json_new_value(JSON_OBJECT, pool);
	json_word_t* plook = word_next(json_doc);
	assert(plook->wtype == '{');
	plook = word_lookup(json_doc);
	while (plook->wtype == WORD_ID
		|| plook->wtype == WORD_STRING)
	{
		json_value_t* json_pair = json_parse_pair(json_doc, pool);
		json_list_push(json_list, json_pair);
		plook = word_lookup(json_doc);
		assert(plook->wtype == '}'
			|| plook->wtype == ',');
		if (plook->wtype == ',')
		{
			word_next(json_doc);
		}

		plook = word_lookup(json_doc);
	}
	assert(plook->wtype == '}');
	word_next(json_doc);
	return json_list;
}

json_value_t* json_parse_array(json_document_t* json_doc, pool_t* pool)
{
	json_value_t* json_list = json_new_value(JSON_ARRAY, pool);
	json_word_t* plook = word_next(json_doc);
	assert(plook->wtype == '[');
	plook = word_lookup(json_doc);
	while (plook->wtype != ']')
	{
		json_value_t* json_val = json_parse_value(json_doc, pool);
		json_list_push(json_list, json_val);
		if (plook->wtype == ',')
		{
			word_next(json_doc);
		}

		plook = word_lookup(json_doc);
	}
	assert(plook->wtype == (int)']');
	word_next(json_doc);
	return json_list;
}

json_value_t* json_parse_id(json_document_t* json_doc, pool_t* pool)
{
	json_value_t* json_val = json_new_value(JSON_ID, pool);
	json_word_t* word = word_next(json_doc);
	assert(json_val->json_type == JSON_STRING || json_val->json_type == JSON_ID);
	if (json_val->json_type == JSON_ID)
	{
		json_set_id_word(json_val, word, pool);
	}
	else
	{
		json_set_string_word(json_val, word, pool);
	}
	return json_val;
}
json_value_t* json_parse_string(json_document_t* json_doc, pool_t* pool)
{
	json_value_t* json_val = json_new_value(JSON_STRING, pool);
	json_word_t*  word = word_next(json_doc);
	assert(word->wtype == WORD_STRING);
	json_set_string_word(json_val, word, pool);
	return json_val;
}

json_value_t* json_parse_int(json_document_t* json_doc, pool_t* pool)
{
	json_value_t* json_val = json_new_value(JSON_INT, pool);
	json_word_t* word = word_next(json_doc);
	assert(word->wtype == WORD_INT);
	json_set_int_word(json_val, word);
	return json_val;
}

json_value_t* json_parse_double(json_document_t* json_doc, pool_t* pool)
{
	json_value_t* json_val = json_new_value(JSON_DOUBLE, pool);
	json_word_t* word = word_next(json_doc);
	assert(word->wtype == WORD_DOUBLE);
	json_set_double_word(json_val, word);
	return json_val;
}

json_value_t* json_parse_value(json_document_t* json_doc, pool_t* pool)
{
	json_word_t* plook = word_lookup(json_doc);
	switch (plook->wtype)
	{
	case '{':
		return json_parse_object(json_doc, pool);
	case '[':
		return json_parse_array(json_doc, pool);
	case WORD_STRING:
		return json_parse_string(json_doc, pool);
	case WORD_INT:
		return json_parse_int(json_doc, pool);
	case WORD_DOUBLE:
		return json_parse_double(json_doc, pool);
	case WORD_ID:
		return json_parse_id(json_doc, pool);
	default:
		return &(json_doc->json_error);
	}
}

json_value_t* json_parse_root(json_document_t* json_doc)
{
	pool_t* pool = pool_create("json_pool", BLOCK_SIZE, BLOCK_SIZE);
	json_doc->memery_pool = pool;
	return json_parse_root_pool(json_doc, pool);
}

json_value_t* json_parse_root_pool(json_document_t* json_doc, pool_t* pool)
{
	assert(json_doc->json != NULL);

	return json_parse_value(json_doc, pool);
}


bool_t        json_is_array(json_value_t* json_val)
{
	return json_val->json_type == JSON_ARRAY;
}

bool_t        json_is_object(json_value_t* json_val)
{
	return json_val->json_type == JSON_OBJECT;
}

bool_t        json_is_string(json_value_t* json_val)
{
	return json_val->json_type == JSON_STRING
		|| json_val->json_type == JSON_ID;
}

bool_t        json_is_int(json_value_t* json_val)
{
	return json_val->json_type == JSON_INT;
}

bool_t		  json_object_haskey(json_value_t* json_val, const char* key)
{
	assert(json_val->json_type == JSON_OBJECT);
	json_value_list_t* json_list = &(json_val->list_val);
	json_value_t*      json_pair = json_list->next;
	while ((json_value_list_t*)json_pair != json_list)
	{
		const char* pair_key = json_pair_getkey_str(json_pair);
		if (strcmp(key, pair_key) == 0)
		{
			return K_TRUE;
		}
		json_pair = json_pair->next;
	}
	return K_FALSE;
}

int           json_as_int(json_value_t* json_val)
{
	assert(json_val->json_type == JSON_INT);
	return json_val->int_val;
}

const char*   json_as_string(json_value_t* json_val)
{
	assert(json_is_string(json_val));
	return json_val->string_val;
}

const char*   json_pair_getkey_str(json_value_t* json_val)
{
	assert(json_val->json_type == JSON_PAIR);
	json_value_t* pair_val = json_val->pair_val.json_key;
	return json_as_string(pair_val);
}

json_value_t* json_pair_getkey(json_value_t* json_val)
{
	assert(json_val->json_type == JSON_PAIR);
	return json_val->pair_val.json_key;
}

json_value_t* json_pair_getval(json_value_t* json_val)
{
	assert(json_val->json_type == JSON_PAIR);
	return json_val->pair_val.json_val;
}

unsigned int  json_child_count(json_value_t* json_val)
{
	assert(json_val->json_type == JSON_OBJECT
		|| json_val->json_type == JSON_ARRAY);
	return list_get_size((list_t*)&(json_val->list_val)) - 1;
}

json_value_t* json_child_get(json_value_t* json_val, unsigned int index)
{
	assert(json_val->json_type == JSON_OBJECT
		|| json_val->json_type == JSON_ARRAY);

	unsigned int cur_index = 0;
	json_value_list_t* json_list = &(json_val->list_val);
	json_value_t* json_child = json_list->next;
	while ((json_value_list_t*)json_child != json_list)
	{
		if (cur_index == index)
		{
			return json_child;
		}
		++cur_index;
		json_child = json_child->next;
	}
	return NULL;
}

json_value_t* json_object_get(json_value_t* json_val, const char* key)
{
	assert(json_val->json_type == JSON_OBJECT);
	json_value_list_t* json_list = &(json_val->list_val);
	json_value_t*      json_pair = json_list->next;
	while ((json_value_list_t*)json_pair != json_list)
	{
		const char* pair_key = json_pair_getkey_str(json_pair);
		if (strcmp(key, pair_key) == 0)
		{
			return json_pair_getval(json_pair);
		}
		json_pair = json_pair->next;
	}
	return NULL;
}

json_value_t* json_child_first(json_value_t* json_val)
{
	assert(json_val->json_type == JSON_OBJECT
		|| json_val->json_type == JSON_ARRAY);

	json_value_list_t* json_list = &(json_val->list_val);
	json_value_t*      json_child = json_list->next;
	if (json_list != (json_value_list_t*)json_child)
	{
		return json_child;
	}
	return NULL;
}

json_value_t* json_child_next(json_value_t* json_val, json_value_t* json_child)
{
	assert(json_val->json_type == JSON_OBJECT
		|| json_val->json_type == JSON_ARRAY);

	json_value_list_t* json_list = &(json_val->list_val);
	json_value_t*      json_next = json_child->next;
	if (json_list != (json_value_list_t*)json_next)
	{
		return json_next;
	}
	return NULL;
}

static const char* json_get_quote(json_document_t* json_doc)
{
	return json_doc->double_quote ? "\"" : "'";
}

bool_t        json_print_node(json_document_t* json_doc, json_value_t* json_val, print_json_callback print_back)
{
	if (print_back == NULL)
	{
		return K_FALSE;
	}
	switch (json_val->json_type)
	{
	case JSON_INT:
		return json_print_int(json_doc, json_val, print_back);
	case JSON_DOUBLE:
		return json_print_double(json_doc, json_val, print_back);
	case JSON_STRING:
		return json_print_string(json_doc, json_val, print_back);
	case JSON_ID:
		return json_print_key(json_doc, json_val, print_back);
	case JSON_BOOL:
		return json_print_bool(json_doc, json_val, print_back);
	case JSON_NULL:
		return json_print_null(json_doc, json_val, print_back);
	case JSON_ARRAY:
		return json_print_array(json_doc, json_val, print_back);
	case JSON_OBJECT:
		return json_print_object(json_doc, json_val, print_back);
	case JSON_PAIR:
		return json_print_pair(json_doc, json_val, print_back);
	default:
		return K_FALSE;
	}
}

bool_t        json_print_int(json_document_t* json_doc, json_value_t* json_val, print_json_callback print_back)
{
	assert(json_val->json_type == JSON_INT);
	char str[16];
	sprintf(str, "%d", json_val->int_val);
	print_back(str);
	return K_TRUE;
}

bool_t        json_print_double(json_document_t* json_doc, json_value_t* json_val, print_json_callback print_back)
{
	assert(json_val->json_type == JSON_DOUBLE);
	char str[16];
	sprintf(str, "%f", json_val->double_val);
	print_back(str);
	return K_TRUE;
}

bool_t        json_print_string(json_document_t* json_doc, json_value_t* json_val, print_json_callback print_back)
{
	assert(json_val->json_type == JSON_STRING || json_val->json_type == JSON_ID);
	const char* quote = json_get_quote(json_doc);
	print_back(quote);
	print_back(json_val->string_val);
	print_back(quote);
	return K_TRUE;
}
bool_t        json_print_array(json_document_t* json_doc, json_value_t* json_val, print_json_callback print_back)
{
	assert(json_val->json_type == JSON_ARRAY);
	print_back("[");

	json_value_t* json_child = json_child_first(json_val);
	while (json_child != NULL)
	{
		json_print_node(json_doc, json_child, print_back);
		json_child = json_child_next(json_val, json_child);
		if (json_child != NULL)
		{
			print_back(",");
		}
	}
	print_back("]");
	return K_TRUE;
}
bool_t        json_print_object(json_document_t* json_doc, json_value_t* json_val, print_json_callback print_back)
{
	assert(json_val->json_type == JSON_OBJECT);
	print_back("{");
	json_value_t* json_child = json_child_first(json_val);
	while (json_child != NULL)
	{
		json_print_pair(json_doc, json_child, print_back);
		json_child = json_child_next(json_val, json_child);
		if (json_child != NULL)
		{
			print_back(",");
		}
	}
	print_back("}");
	return K_TRUE;
}

bool_t        json_print_key(json_document_t* json_doc, json_value_t* json_val, print_json_callback print_back)
{
	assert(json_val->json_type == JSON_ID);
	if (json_doc->quote_key){
		json_print_string(json_doc, json_val, print_back);
	}
	else{
		print_back(json_val->string_val);
	}
	
	return K_TRUE;
}

bool_t        json_print_bool(json_document_t* json_doc, json_value_t* json_val, print_json_callback print_back)
{
	assert(json_val->json_type == JSON_BOOL);
	print_back(json_val->bool_val ? "true" : "false");
	return K_TRUE;
}

bool_t        json_print_null(json_document_t* json_doc, json_value_t* json_val, print_json_callback print_back)
{
	assert(json_val->json_type == JSON_NULL);
	print_back("null");
	return K_TRUE;
}

bool_t        json_print_pair(json_document_t* json_doc,json_value_t* json_val, print_json_callback print_back)
{
	assert(json_val->json_type == JSON_PAIR);
	json_value_t* pair_key = json_pair_getkey(json_val);
	json_print_key(json_doc, pair_key, print_back);
	print_back(":");
	json_value_t* pair_val = json_pair_getval(json_val);
	json_print_node(json_doc, pair_val, print_back);
	return K_TRUE;
}


json_value_t* json_create_value(json_document_t* json_doc, json_type_t json_type)
{
	json_value_t* val = json_new_value(json_type, json_doc->memery_pool);
	return val;
}

json_value_t* json_create_object(json_document_t* json_doc)
{
	return json_create_value(json_doc, JSON_OBJECT);
}

json_value_t* json_create_array(json_document_t* json_doc)
{
	return json_create_value(json_doc, JSON_ARRAY);
}
json_value_t* json_create_id(json_document_t* json_doc, const char* str_val)
{
	size_t str_len = strlen(str_val);
	json_value_t* json_val = json_create_value(json_doc, JSON_ID);
	json_val->string_val = (char*)pool_malloc(json_doc->memery_pool, str_len + 1);
	memcpy(json_val->string_val, str_val, str_len);
	(json_val->string_val)[str_len] = '\0';
	return json_val;
}

json_value_t* json_create_string(json_document_t* json_doc, const char* str_val)
{
	json_value_t* json_val = json_create_id(json_doc, str_val);
	json_val->json_type = JSON_STRING;
	return json_val;
}

json_value_t* json_create_int(json_document_t* json_doc, int int_val)
{
	json_value_t* json_val = json_create_value(json_doc, JSON_INT);
	json_val->int_val = int_val;
	return json_val;
}
json_value_t* json_create_double(json_document_t* json_doc, double double_val)
{
	json_value_t* json_val = json_create_value(json_doc, JSON_DOUBLE);
	json_val->double_val = double_val;
	return json_val;
}


bool_t        json_add_val2obj(json_document_t* json_doc,json_value_t* obj, json_value_t* key, json_value_t* val)
{
	assert(obj->json_type == JSON_OBJECT);
	json_value_t* pair = json_new_value(JSON_PAIR, json_doc->memery_pool);
	pair->pair_val.json_key = key;
	pair->pair_val.json_val = val;
	json_list_push(obj, pair);
	return K_TRUE;
}

bool_t        json_add_val2arr(json_value_t* arr, json_value_t* val)
{
	assert(arr->json_type == JSON_ARRAY);
	json_list_push(arr, val);
	return K_TRUE;
}