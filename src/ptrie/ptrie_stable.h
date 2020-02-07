/* 
 * Copyright (C) 2016  Peter Gjøl Jensen <root@petergjoel.dk>
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/* 
 * File:   ptrie_map.h
 * Author: Peter G. Jensen
 *
 *
 * Created on 06 October 2016, 13:51
 */

#ifndef PTRIE_MAP_H
#define PTRIE_MAP_H
#include "ptrie.h"


namespace ptrie {

    template<
    typename KEY = unsigned char,
    uint16_t HEAPBOUND = 128,
    uint16_t SPLITBOUND = 128,
    size_t ALLOCSIZE = (1024 * 64),
    typename T = void,
    typename I = size_t
    >
    class set_stable : public set<KEY, HEAPBOUND, SPLITBOUND, ALLOCSIZE, T, I> {
#ifdef __APPLE__
#define pt set<HEAPBOUND, SPLITBOUND, ALLOCSIZE, T, I>
#else
        using pt = set<KEY, HEAPBOUND, SPLITBOUND, ALLOCSIZE, T, I>;
#endif
    public:
        set_stable() : pt()
        {
            this->_entries = std::make_unique<linked_bucket_t<typename pt::entry_t, ALLOCSIZE>>(1);
        }
        
        set_stable(set_stable&&) = default;
        set_stable& operator=(set_stable&&) = default;
        
        size_t size() const {
            return this->_entries->size();
        }

        size_t unpack(I index, KEY* destination) const;
        std::vector<KEY> unpack(I index) const;
    protected:
        typename set<KEY, HEAPBOUND, SPLITBOUND, ALLOCSIZE, T, I>::node_t* find_metadata(I index, std::stack<uchar>& path, size_t& bindex, size_t& offset, size_t& ps, uint16_t& size) const;
        void write_data(KEY* destination, typename pt::node_t* node, std::stack<uchar>& path, size_t& bindex, size_t& offset, size_t& ps, uint16_t& size) const;
  };

    template<PTRIETPL>
    typename set<KEY, HEAPBOUND, SPLITBOUND, ALLOCSIZE, T, I>::node_t* 
    set_stable<KEY, HEAPBOUND, SPLITBOUND, ALLOCSIZE, T, I>::
    find_metadata(I index, std::stack<uchar>& path, size_t& bindex, size_t& offset, size_t& ps, uint16_t& size) const
    {
        typename pt::node_t* node = nullptr;
        typename pt::fwdnode_t* par = nullptr;
        // we can find size without bothering anyone (to much)        
        
        bindex = 0;
        {
#ifndef NDEBUG
            bool found = false;
#endif
            typename pt::entry_t& ent = this->_entries->operator[](index);
            par = (typename pt::fwdnode_t*)ent.node;
            node = (typename pt::node_t*)par->_children[ent.path];
            typename pt::bucket_t* bckt = node->_data;
            I* ents = bckt->entries(node->_count, true);
            for (size_t i = 0; i < node->_count; ++i) {
                if (ents[i] == index) {
                    bindex = i;
#ifndef NDEBUG
                    found = true;
#endif
                    break;
                }
            }
            assert(found);
        }



        while (par != this->_root.get()) {
            path.push(par->_path);
            par = par->_parent;
        }

        size = 0;
        offset = 0;
        ps = path.size();
        if (ps <= 1) {
            size = node->_data->first(0, bindex);
            if (ps == 1) {
                size >>= 8;
                uchar* bs = (uchar*) & size;
                bs[1] = path.top();
                path.pop();
            }

            uint16_t o = size;
            for (size_t i = 0; i < bindex; ++i) {

                uint16_t f = node->_data->first(0, i);
                uchar* fc = (uchar*) & f;
                uchar* oc = (uchar*) & o;
                if (ps != 0) {
                    f >>= 8;
                    fc[1] = oc[1];
                    f -= 1;
                }
                offset += pt::bytes(f);
            }
        } else {
            uchar* bs = (uchar*) & size;
            bs[1] = path.top();
            path.pop();
            bs[0] = path.top();
            path.pop();
            offset = (pt::bytes(size - ps) * bindex);
        }        
        return node;
    }
    
    template<PTRIETPL>
    void
    set_stable<KEY, HEAPBOUND, SPLITBOUND, ALLOCSIZE, T, I>::write_data(KEY* dest, typename pt::node_t* node, std::stack<uchar>& path, size_t& bindex, size_t& offset, size_t& ps, uint16_t& size) const
    {
        if (size > ps) {
            uchar* src;
            if ((size - ps) >= HEAPBOUND) {
                src = *((uchar**)&(node->_data->data(node->_count, true)[offset]));
            } else {
                src = &(node->_data->data(node->_count, true)[offset]);
            }

            if constexpr (byte_iterator<KEY>::continious())
                memcpy(&byte_iterator<KEY>::access(dest, ps), src, (size - ps));
            else
                for(size_t i = 0; i < (size - ps); ++i)
                    byte_iterator<KEY>::access(dest, ps + i) = src[i];
        }

        uint16_t first = node->_data->first(0, bindex);

        size_t pos = 0;
        while (!path.empty()) {
            byte_iterator<KEY>::access(dest, pos) = path.top();
            path.pop();
            ++pos;
        }


        if (ps > 0) {
            uchar* fc = (uchar*) & first;
            if (ps > 1) {
                byte_iterator<KEY>::access(dest, pos) = fc[1];
                ++pos;
            }
            byte_iterator<KEY>::access(dest, pos) = fc[0];
            ++pos;
        }        
    }
  
    template<PTRIETPL>
    size_t
    set_stable<KEY, HEAPBOUND, SPLITBOUND, ALLOCSIZE, T, I>::unpack(I index, KEY* dest) const {
        size_t bindex, ps, offset;
        uint16_t size;
        std::stack<uchar> path;
        auto node = find_metadata(index, path, bindex, offset, ps, size);
        write_data(dest, node, path, bindex, offset, ps, size);        
        return size/sizeof(KEY);
    }
    
    template<PTRIETPL>
    std::vector<KEY>
    set_stable<KEY, HEAPBOUND, SPLITBOUND, ALLOCSIZE, T, I>::unpack(I index) const {
        size_t bindex, ps, offset;
        uint16_t size;
        std::stack<uchar> path;
        auto node = find_metadata(index, path, bindex, offset, ps, size);
        std::vector<KEY> destination(size/sizeof(KEY));
        write_data(destination.data(), node, path, bindex, offset, ps, size);        
        return destination;   
    }
    
}

#undef pt
#endif /* PTRIE_MAP_H */
