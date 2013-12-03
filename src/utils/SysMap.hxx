/** \copyright
 * Copyright (c) 2013, Stuart W Baker
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are  permitted provided that the following conditions are met:
 * 
 *  - Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 *  - Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * \file SysMap.hxx
 * This file provides a C++ abstraction of BSD sys/tree.h.
 *
 * @author Stuart W. Baker
 * @date 2 December 2013
 */

#ifndef _SysMap_hxx_
#define _SysMap_hxx_

#include <sys/tree.hxx>

#include "utils/macros.h"

/** This is an abstraction of BSD sys/tree.h that is meant to mimic the
 * semantics of std::map without having to link in libstdc++.
 */
template <typename Key, typename Value> class SysMap
{
public:
    /** Default Constructor which with no mapping entry limit.
     */
    SysMap()
        : entries(0),
          used(0),
          freeList(NULL)

    {
        RB_INIT(&head);
    }

    /** Constructor that limits the number of mappings to a static pool.
     * @param entries number of nodes to statically create and track
     */
    SysMap(size_t entries) 
        : entries(entries),
          used(0),
          freeList(NULL)
    {
        RB_INIT(&head);
        Node *first = new Node[entries];
        first->entry.rbe_left = (Node*)this;
        
        for (size_t i = 1; i < entries; i++)
        {
            first[i].entry.rbe_left = freeList;
            freeList = first + i;
        }
    }

    /** Destructor.
     */
    ~SysMap()
    {
        /* We should never get here */
        /*  why?? HASSERT(0); */
    }
    
    /** mimic std::pair */
    struct Pair
    {
        Key first; /**< mimic first element in an std::pair */
        Value second; /**< mimic second element in an std::pair */
    };

private:
    /** The metadata for a tree node. */
    class Node
    {
    public:
        RB_ENTRY(Node) entry;

        union
        {
            Pair p; /**< pair of element */
            struct
            {
                Key key; /**< key by which to sort the node */
                Value value; /**< value of the node */
            };
        };

        /** Default constructor.  Does not initialize key or value.
         */
        Node()
        {
        }
        
        /** Constructor.
         * @param key initial value of key
         * @param value initial value of value
         */
        Node(Key key, Value value)
            : key(key),
              value(value)
        {
        }
    };

public:
    /** This mimics an std::iterator.
     */
    class Iterator
    {
    public:
        /** Default constructor.
         */
        Iterator()
            : node(NULL),
              m(NULL)
        {
        }
        
        /** Copy constructor.
         */
        Iterator(const Iterator &it)
            : node(it.node),
              m(it.m)
        {
        }
        
        /** Constructor.
         * @param context context passed in at instantiation
         * @param node node to initialize this iteration with
         */
        Iterator(SysMap* context, Node *node)
            : node(node),
              m(context)
            
        {
        }

        /** Destructor */
        ~Iterator()
        {
        }
        
        /** Overloaded reference operator.
         */
        Pair& operator*() const
        {
            return node->p;
        }

        /** Overloaded pre-increement operator. */
        Iterator& operator ++ ()
        {
            if (node)
            {
                node = RB_NEXT(m->tree, &(m->head), node);
            }
            return *this;
        }

        /** Overloaded not equals operator. */
        bool operator != (const Iterator& it)
        {
            return node != it.node;
        }

        /** Overloaded equals operator. */
        bool operator == (const Iterator& it)
        {
            return node == it.node;
        }

    private:
        /** node this iteration is currently indexed to */
        Node *node;
        
        /** Context this iteration lives in */
        SysMap *m;

        /** Allow access to node member */
        friend class SysMap;
    };
    
    /** Find the index associated with the key and create it if does not exist.
     * @param key key to lookup
     * @return value of the key by reference
     */
    Value& operator[](const Key &key)
    {
        Iterator it = find(key);
        if (it == end())
        {
            Node *node = alloc();
            HASSERT(node);
            node->key = key;
            node->value = 0;
            RB_INSERT(tree, &head, node);
            it = Iterator(this, node);
        }
        return (*it).second;
    }

    /** Number of elements currently in the map.
     * @return number of elements in the map
     */
    size_t size()
    {
        return used;
    }

    /** Maximum theoretical number of elements in the map.
     * @return maximum theoretical number of elements in the map
     */
    size_t max_size()
    {
        return entries;
    }
    
    /** Find an element matching the given key.
     * @param key key to search for
     * @return iterator index pointing to key, else iterator end() if not found
     */
    Iterator find( const Key &key )
    {
        Node lookup;
        lookup.key = key;
        Node *node = RB_FIND(tree, &head, &lookup);
        
        return node ? Iterator(this, node) : Iterator();
    }
    
    /** Get an iterator index pointing one past the last element in mapping.
     * @return iterator index pointing to one past the last element in mapping
     */
    Iterator end()
    {
        return Iterator();
    }

    /** Get an iterator index pointing to the first element in the mapping.
     * @return iterator index pointing to the first element in the mapping or
     * to end() if the mapping is empty
     */
    Iterator begin()
    {
        Node *node = RB_MIN(tree, &head);
        
        return node ? Iterator(this, node) : Iterator();
    }

    /** Remove a node from the tree.
     * @param key key for the element to remove
     * @return number of elements removed
     */
    size_t erase(Key key)
    {
        Node lookup;
        lookup.key = key;
        Node *node = RB_FIND(tree, &head, &lookup);

        if (node)
        {
            RB_REMOVE(tree, &head, node);
            free(node);
            return 1;
        }
        return 0;
    }
    
    /** Remove a node from the tree.
     * @param it iterator index for the element to remove
     */
    void erase(Iterator it)
    {
        Node *node = it.node;
        if (node)
        {
            RB_REMOVE(tree, &head, node);
            free(node);
        }
    }

private:
    /** Allocate a node from the free list.
     * @return newly allocated node, else NULL if no free nodes left
     */
    Node *alloc()
    {
        if (freeList && freeList != (Node*)this)
        {
            Node *node = freeList;
            freeList = freeList->entry.rbe_left;
            ++used;
            return node;
        }
        return NULL;
    }
    
    /** free a node to the free list if it exists.
     * @param node node to free
     */
    void free(Node *node)
    {
        if (freeList || freeList == (Node*)this)
        {
            node->entry.rbe_left = freeList;
            freeList = node;
            --used;
        }
    }

    /** Compare two nodes.
     * @param a first of two nodes to compare
     * @param b second of two nodes to compare
     * @return difference between node keys (a->key - b->key)
     */
    Key compare(Node *a, Node *b)
    {
        return a->key - b->key;
    }

    /** total number of entries for this instance */
    size_t entries;

    /** total number of entries in use for this instance */
    size_t used;

    /** list of free nodes */
    Node *freeList;

    /** The datagram tree type. */
    RB_HEAD(tree, Node);

    /** The datagram tree methods. */
    RB_GENERATE(tree, Node, entry, compare);
    
    /** tree instance */
    struct tree head;

    DISALLOW_COPY_AND_ASSIGN(SysMap);
};

#endif /* _SysMap_hxx_ */
