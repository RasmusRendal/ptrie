/*
 * Copyright (C) 2016  Peter G. Jensen <root@petergjoel.dk>
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
// Created by Peter G. Jensen on 24/12/16.

#include <iostream>
#include <ptrie.h>
#include <stdlib.h>
#include <sparsehash/sparse_hash_set>
#include <sparsehash/dense_hash_set>
#include <tbb/concurrent_unordered_set.h>
#include <random>
#include <ptrie.h>
#include <chrono>
#include <unordered_set>
#include "MurmurHash2.h"
#include "utils.h"

auto reorder(auto el)
{
    unsigned char noise = 0b10101010;
    auto target = el;
    unsigned char* t = (unsigned char*)&target;
    unsigned char* s = (unsigned char*)&el;
    for(size_t i = 0; i < sizeof(el); ++ i)
    {
        t[i] = s[sizeof(el) - (i + 1)]/* xor noise*/;
    }
    return target;
}


void set_insert(auto& set, size_t elements, size_t seed, double deletes, double read_rate)
{
    std::default_random_engine generator(seed);
    std::uniform_real_distribution<double> dist;

    std::default_random_engine read_generator(seed);
    std::normal_distribution<double> read_dist(read_rate, read_rate / 2.0);
    std::uniform_int_distribution<uint32_t> read_el(1, elements);


    auto start = std::chrono::system_clock::now();
    for(uint32_t i = 1; i <= elements; ++i)
    {
        uint32_t val = reorder(i);
        set.insert(val);

        if(read_rate > 0.0)
        {
            int reads = std::round(read_dist(read_generator));
            for(int r = 0; r < reads; ++r)
            {
                size_t el = reorder(read_el(read_generator));
                set.count(el);
            }
        }

        if(dist(generator) < deletes)
        {
            std::uniform_int_distribution<uint32_t>  rem(1, i);
            size_t el = reorder(rem(generator));
            auto it = set.find(el);
            if(it != set.end())
                set.erase(it);
        }
    }
    auto end = std::chrono::system_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    std::cout << "COMPLETED IN " << (0.001*elapsed.count()) << " SECONDS " << std::endl;
}

void set_insert_ptrie(auto& set, size_t elements, size_t seed, double deletes, double read_rate)
{
    std::default_random_engine del_generator(seed);
    std::uniform_real_distribution<double> del_dist;

    std::default_random_engine read_generator(seed);
    std::normal_distribution<double> read_dist(read_rate, read_rate / 2.0);
    std::uniform_int_distribution<uint32_t > read_el(1, elements);
    auto start = std::chrono::system_clock::now();

    for(uint32_t i = 1; i <= elements; ++i)
    {
        uint32_t val = reorder(i);
        set.insert((unsigned char*)&val, sizeof(val));

        if(read_rate > 0.0)
        {
            int reads = std::round(read_dist(read_generator));
            for(int r = 0; r < reads; ++r)
            {
                uint32_t el = reorder(read_el(read_generator));
                set.exists((unsigned char*)&el, sizeof(el));
            }
        }
        if(del_dist(del_generator) < deletes)
        {

            std::uniform_int_distribution<uint32_t>  del_el(1, elements);
            uint32_t el = reorder(del_el(del_generator));
            set.erase((unsigned char*)&el, sizeof(el));
        }
    }
    auto end = std::chrono::system_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    std::cout << "COMPLETED IN " << (0.001*elapsed.count()) << " SECONDS " << std::endl;
}


struct hasher_o
{
    uint64_t operator()(const size_t& w) const
    {
        return MurmurHash64A(&w, sizeof(size_t), 0);
    }
};

struct equal_o
{
    bool operator()(const size_t& w1, const size_t& w2) const
    {
        return w1 == w2;
    }
};

int main(int argc, const char** argv)
{
    if(argc < 3 || argc > 8)
    {
        std::cout << "usage : <ptrie/std/sparse/dense> <number elements> <?seed> <?delete ratio> <?read rate>" << std::endl;
        exit(-1);
    }

    const char* type = argv[1];
    size_t elements = 1024;
    size_t seed = 0;
    double deletes = 0.0;
    double read_rate = 0.0;

    read_arg(argv[2], elements, "Error in <number of elements>", "%zu");
    if(argc > 3) read_arg(argv[3], seed, "Error in <seed>", "%zu");
    if(argc > 4) read_arg(argv[4], deletes, "Error in <delete ratio>", "%lf");
    if(argc > 5) read_arg(argv[5], read_rate, "Error in <read rate>", "%lf");

    if(strcmp(type, "ptrie") == 0)
    {
        print_settings(type, elements, seed, sizeof(size_t), deletes, read_rate, 256);
        ptrie::set<> set;
        set_insert_ptrie(set, elements, seed, deletes, read_rate);
    }
    /*else if (strcmp(type, "std") == 0) {
        print_settings(type, elements, seed, sizeof(size_t), deletes, read_rate, 256);
        std::unordered_set<size_t, hasher_o, equal_o> set;
        set_insert(set, elements, seed, deletes, read_rate);
    }*/
    /*else if(strcmp(type, "tbb") == 0)
    {
        print_settings(type, elements, seed, sizeof(size_t), deletes, read_rate, 256);
        tbb::concurrent_unordered_set<size_t, hasher_o, equal_o> set;
        set_insert(set, elements, seed, deletes, read_rate);
    }*/
    else if(strcmp(type, "sparse") == 0)
    {
        print_settings(type, elements, seed, sizeof(size_t), deletes, read_rate, 256);
        google::sparse_hash_set<uint32_t , hasher_o, equal_o> set(elements/10);
        set_insert(set, elements, seed, deletes, read_rate);
    }
    else if(strcmp(type, "dense") == 0)
    {
        print_settings(type, elements, seed, sizeof(size_t), deletes, read_rate, 256);
        google::dense_hash_set<uint32_t , hasher_o, equal_o> set(elements/10);
        set.set_empty_key(reorder(0));
        if(deletes > 0.0) set.set_deleted_key(reorder(std::numeric_limits<uint32_t>::max()));
        set_insert(set, elements, seed, deletes, read_rate);
    }
    else
    {
        std::cerr << "ERROR IN TYPE, ONLY VALUES ALLOWED : ptrie, std, sparse, dense, tbb" << std::endl;
        exit(-1);
    }

    return 0;
}