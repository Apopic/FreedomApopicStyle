#pragma once

namespace exmath {
	// 10進数での桁数を返す (0 は 1 桁)
	template<class T>
	inline int digit(T Num) {
		int digit = 1;
		while (Num /= 10) {
			++digit;
		}
		return digit;
	}
}