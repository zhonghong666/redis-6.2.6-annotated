/* adlist.c - A generic doubly linked list implementation
 *
 * Copyright (c) 2006-2010, Salvatore Sanfilippo <antirez at gmail dot com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *   * Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *   * Neither the name of Redis nor the names of its contributors may be used
 *     to endorse or promote products derived from this software without
 *     specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */


#include <stdlib.h>
#include "adlist.h"
#include "zmalloc.h"

/* Create a new list. The created list can be freed with
 * listRelease(), but private value of every node need to be freed
 * by the user before to call listRelease(), or by setting a free method using
 * listSetFreeMethod.
 *
 * On error, NULL is returned. Otherwise the pointer to the new list. */
list *listCreate(void)
{
    struct list *list;

    if ((list = zmalloc(sizeof(*list))) == NULL) // 申请内存
        return NULL;
    list->head = list->tail = NULL;
    list->len = 0;
    list->dup = NULL;
    list->free = NULL;
    list->match = NULL;
    return list;
}

/* Remove all the elements from the list without destroying the list itself. */
void listEmpty(list *list)
{
    unsigned long len;
    listNode *current, *next;

    current = list->head;
    len = list->len;
    while(len--) {
        next = current->next;
        // 如果定义了节点值释放函数，需要调用
        if (list->free) list->free(current->value);
        zfree(current); // 释放当前节点
        current = next;
    }
    list->head = list->tail = NULL;
    list->len = 0;
}

/* Free the whole list.
 * 释放整个链表
 * This function can't fail. */
void listRelease(list *list)
{
    listEmpty(list);
    zfree(list);
}

/* Add a new node to the list, to head, containing the specified 'value'
 * pointer as value.
 *
 * On error, NULL is returned and no operation is performed (i.e. the
 * list remains unaltered).
 * On success the 'list' pointer you pass to the function is returned. */
// 向list的头部插入一个节点
list *listAddNodeHead(list *list, void *value)
{
    listNode *node;

    if ((node = zmalloc(sizeof(*node))) == NULL) // 申请内存
        return NULL;
    node->value = value;
    if (list->len == 0) { // 链表为空
        list->head = list->tail = node;
        node->prev = node->next = NULL;
    } else { // 链表非空
        node->prev = NULL;
        node->next = list->head;
        list->head->prev = node;
        list->head = node;
    }
    list->len++; // 长度 + 1
    return list;
}

/* Add a new node to the list, to tail, containing the specified 'value'
 * pointer as value.
 *
 * On error, NULL is returned and no operation is performed (i.e. the
 * list remains unaltered).
 * On success the 'list' pointer you pass to the function is returned. */
// 向list尾部插入一个节点
list *listAddNodeTail(list *list, void *value)
{
    listNode *node;

    if ((node = zmalloc(sizeof(*node))) == NULL) // 申请内存
        return NULL;
    node->value = value;
    if (list->len == 0) { // 链表为空
        list->head = list->tail = node;
        node->prev = node->next = NULL;
    } else { // 链表非空
        node->prev = list->tail;
        node->next = NULL;
        list->tail->next = node;
        list->tail = node;
    }
    list->len++; // 长度 + 1
    return list;
}
/*
 * 向任意位置插入节点
 * 其中，old_node: 插入位置
 *      value: 插入节点的值
 *      after: 0 表示插入到old_node的前面，1 表示插入到old_node后面
 */
list *listInsertNode(list *list, listNode *old_node, void *value, int after) {
    listNode *node;

    if ((node = zmalloc(sizeof(*node))) == NULL) // 申请内存
        return NULL;
    node->value = value;
    if (after) {
        node->prev = old_node;
        node->next = old_node->next;
        if (list->tail == old_node) {
            list->tail = node;
        }
    } else {
        node->next = old_node;
        node->prev = old_node->prev;
        if (list->head == old_node) {
            list->head = node;
        }
    }
    if (node->prev != NULL) {
        node->prev->next = node;
    }
    if (node->next != NULL) {
        node->next->prev = node;
    }
    list->len++; // 长度 + 1
    return list;
}

/* Remove the specified node from the specified list.
 * It's up to the caller to free the private value of the node.
 *
 * This function can't fail. */
// 删除指定节点
void listDelNode(list *list, listNode *node)
{
    if (node->prev) // 不是头节点
        node->prev->next = node->next;
    else
        list->head = node->next;
    if (node->next) // 不是尾节点
        node->next->prev = node->prev;
    else
        list->tail = node->prev;
    // 如果指定了节点值释放函数，需要调用
    if (list->free) list->free(node->value);
    zfree(node); // 释放该节点内存
    list->len--; // 长度 - 1
}

/* Returns a list iterator 'iter'. After the initialization every
 * call to listNext() will return the next element of the list.
 *
 * This function can't fail. */
// 获取迭代器
listIter *listGetIterator(list *list, int direction)
{
    listIter *iter;
    // 申请内存
    if ((iter = zmalloc(sizeof(*iter))) == NULL) return NULL;
    // 根据迭代方向来初始化iter
    if (direction == AL_START_HEAD)
        iter->next = list->head;
    else
        iter->next = list->tail;
    iter->direction = direction;
    return iter;
}

/* Release the iterator memory */
// 释放迭代器内存
void listReleaseIterator(listIter *iter) {
    zfree(iter);
}

/* Create an iterator in the list private iterator structure */
// 重置迭代器为正向迭代器
void listRewind(list *list, listIter *li) {
    li->next = list->head;
    li->direction = AL_START_HEAD;
}
// 重置迭代器为逆向迭代器
void listRewindTail(list *list, listIter *li) {
    li->next = list->tail;
    li->direction = AL_START_TAIL;
}

/* Return the next element of an iterator.
 * It's valid to remove the currently returned element using
 * listDelNode(), but not to remove other elements.
 *
 * The function returns a pointer to the next element of the list,
 * or NULL if there are no more elements, so the classical usage
 * pattern is:
 *
 * iter = listGetIterator(list,<direction>);
 * while ((node = listNext(iter)) != NULL) {
 *     doSomethingWith(listNodeValue(node));
 * }
 *
 * */
// 获取下一个迭代器
// 根据direction属性来获取下一个迭代器
listNode *listNext(listIter *iter)
{
    listNode *current = iter->next;

    if (current != NULL) {
        if (iter->direction == AL_START_HEAD)
            iter->next = current->next;
        else
            iter->next = current->prev;
    }
    return current;
}

/* Duplicate the whole list. On out of memory NULL is returned.
 * On success a copy of the original list is returned.
 *
 * The 'Dup' method set with listSetDupMethod() function is used
 * to copy the node value. Otherwise the same pointer value of
 * the original node is used as value of the copied node.
 *
 * The original list both on success or error is never modified. */
// 复制整个list
list *listDup(list *orig)
{
    list *copy;
    listIter iter;
    listNode *node;

    if ((copy = listCreate()) == NULL)
        return NULL;
    // 复制节点值操作函数
    copy->dup = orig->dup;
    copy->free = orig->free;
    copy->match = orig->match;
    // 重置迭代器
    listRewind(orig, &iter);
    // 通过迭代器遍历链表
    while((node = listNext(&iter)) != NULL) {
        void *value;
        // 复制节点
        // 如果定义了dup函数，则按照dup函数来复制节点
        if (copy->dup) {
            value = copy->dup(node->value);
            if (value == NULL) {
                listRelease(copy);
                return NULL;
            }
        } else // 如果没有，直接赋值
            value = node->value;
        // 依次向尾部添加节点
        // 如果失败，释放copy链表申请的内存，返回NULL
        if (listAddNodeTail(copy, value) == NULL) {
            listRelease(copy);
            return NULL;
        }
    }
    return copy;
}

/* Search the list for a node matching a given key.
 * The match is performed using the 'match' method
 * set with listSetMatchMethod(). If no 'match' method
 * is set, the 'value' pointer of every node is directly
 * compared with the 'key' pointer.
 *
 * On success the first matching node pointer is returned
 * (search starts from head). If no matching node exists
 * NULL is returned. */
// 根据给定节点值查找node
// O(N)
listNode *listSearchKey(list *list, void *key)
{
    listIter iter;
    listNode *node;
    // 重置迭代器
    listRewind(list, &iter);
    while((node = listNext(&iter)) != NULL) {
        // 如果定义了match函数，通过match函数来对比
        if (list->match) {
            if (list->match(node->value, key)) {
                return node;
            }
        } else { // 没有定义则直接对比节点值
            if (key == node->value) { // 找到该节点
                return node;
            }
        }
    }
    // 没有找到返回NULL
    return NULL;
}

/* Return the element at the specified zero-based index
 * where 0 is the head, 1 is the element next to head
 * and so on. Negative integers are used in order to count
 * from the tail, -1 is the last element, -2 the penultimate
 * and so on. If the index is out of range NULL is returned. */
// 根据序号来查找node
// O(N)
listNode *listIndex(list *list, long index) {
    listNode *n;
    // 序号为负，倒序查找
    if (index < 0) {
        index = (-index)-1;
        n = list->tail;
        while(index-- && n) n = n->prev;
    } else { // 正序查找
        n = list->head;
        while(index-- && n) n = n->next;
    }
    return n;
}

/* Rotate the list removing the tail node and inserting it to the head. */
// 旋转列表，删除尾部节点并将其插入头部
void listRotateTailToHead(list *list) {
    if (listLength(list) <= 1) return;

    /* Detach current tail */
    listNode *tail = list->tail;
    list->tail = tail->prev;
    list->tail->next = NULL;
    /* Move it as head */
    list->head->prev = tail;
    tail->prev = NULL;
    tail->next = list->head;
    list->head = tail;
}

/* Rotate the list removing the head node and inserting it to the tail. */
// 旋转列表，删除头部节点并将其插入到尾部
void listRotateHeadToTail(list *list) {
    if (listLength(list) <= 1) return;

    listNode *head = list->head;
    /* Detach current head */
    list->head = head->next;
    list->head->prev = NULL;
    /* Move it as tail */
    list->tail->next = head;
    head->next = NULL;
    head->prev = list->tail;
    list->tail = head;
}

/* Add all the elements of the list 'o' at the end of the
 * list 'l'. The list 'other' remains empty but otherwise valid. */
// 将列表o的所有元素添加到列表l
// 列表o会变成空list
void listJoin(list *l, list *o) {
    if (o->len == 0) return;

    o->head->prev = l->tail;

    if (l->tail) // 列表l不为空
        l->tail->next = o->head;
    else // 列表l为空
        l->head = o->head;

    l->tail = o->tail;
    l->len += o->len;

    /* Setup other as an empty list. */
    // 将列表o设置成空列表
    o->head = o->tail = NULL;
    o->len = 0;
}
