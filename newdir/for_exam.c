u_int page_perm_stat(Pde *pgdir, struct Page *pp, u_int perm_mask) {
	int count = 0;//统计满足条件的页表项的数量
	Pde *pde;
	Pte *pte;
	for (int i = 0; i < 1024; i++) {
		pde = pgdir + i;
		if(!(*pde & PTE_V)) { //当前页目录是否有效
			continue;
		}
	
		for (int j = 0; j< 1024;j++ ){
			pte = (Pte*)KADDR(PTE_ADDR(*pde)) + j;
			if (!(*pte & PTE_V)) { ////当前页表是否有效
				continue;
			}
			if (((perm_mask | (*pte))== (*pte)) 
                && (((u_long)(page2pa(pp)))>>12) == (((u_long)(PTE_ADDR(*pte)))>>12))
				count++;
            /*该层if判断条件等价于
            (perm_mask & (*pte))== perm_mask
            (page2pa(pp) == PTE_ADDR(*pte))
            */
		}
    }
	return count;
}	
