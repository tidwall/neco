#ifdef NECO_TESTING

void test_units_xx3(void) {
    char data[] = "9eee31a1e38aec186198c742099ca26ade152235a522022a";
    uint64_t exp [] = { 
        0xef46db3751d8e999, 0x3073eed68c0560e3, 0xc30c0e04a049d500, 
        0x65684ed7172a9aab, 0x5d70724192470c5b, 0x5fba524e0a69edaa, 
        0xf71c1a44887e43e1, 0xd2e66ac2e37498c3, 0x1a886c36a17db3f5, 
        0x5784037bdd6ebcf8, 0xb9ef4fbef461e0ed, 0xe3069db7083b35d9, 
        0x444015ec7d677317, 0x175cbcbac99b956d, 0xee241a9316896abc, 
        0x97871dea5dfa15d5, 0xf766d4c3cf79dcf0, 0xbc84ae6c23db3bab, 
        0xc534607cce3e89b2, 0x251097654bb75473, 0xdeb675823fc57e68, 
        0xf706d921b1ef8077, 0xe167a86e0d5b2488, 0x597f1ce8bc96d5d6, 
        0x1d9318a34a79339a, 0xa5493c035962815f, 0x6e0e55435b1d597e, 
        0x2c375597755fb161, 0x31f688cc8cc53c7e, 0xc61af81f53d19a66, 
        0x67e315810fe0f864, 0x433e6791508ecd01, 0x265bb8168c9cf315, 
        0x0ef768f5bd9bdb81, 0xc1fa6b650b855ad5, 0xd45efccc80c8f911, 
        0x704547f49c38ef28, 0x052b89cd32eb6959, 0xd6377bd64d0877dd, 
        0x35649174351f9002, 0x32e423a14079407c, 0x6d9f7ae7b33e4e75, 
        0x4b1312a97c446756, 0x55e56172079276d7, 0xdc5c69cc17b01ffa, 
        0x6b62e86a3d457048, 0xedf90932412081b0, 0x24bd1679a4c82fe7, 
    };
    for (size_t i = 0; i < strlen(data); i++) {
        assert(xxh3(data, i, i) == exp[i]);
    }
}

void test_units_align(void) {
    for (int i = 0; i <= 16; i++)  {
        assert(align_size(i, 16) == 16);
    }
    for (int i = 17; i <= 32; i++)  {
        assert(align_size(i, 16) == 32);
    }
}

void test_units_mmap(void) {
    void *mem = mmap_alloc(1);
    assert(mem);
    mmap_free(mem, 1);
    assert(mmap_alloc(__SIZE_MAX__) == NULL);
}

#endif // NECO_TEST_PRIVATE_FUNCTIONS
