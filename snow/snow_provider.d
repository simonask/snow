provider SnowProbe {
	probe gc_alloc(uint64_t size);
	probe gc();
	probe gc_minor();
	probe gc_major();
	probe gc_finished(uint64_t total, int64_t freed);
};
