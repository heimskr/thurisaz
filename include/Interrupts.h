#pragma once

extern "C" {
	void int_system();
	void int_timer();
	void int_pagefault();
	void int_inexec();
	void int_bwrite();
	void int_keybrd();
}
