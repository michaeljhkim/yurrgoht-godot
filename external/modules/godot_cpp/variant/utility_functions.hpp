#pragma once

#include "core/variant/variant_utility.h"

#include "godot_cpp/wrapper_constants.h"

namespace godot {
	template <typename... Args>
	static void UtilityFunctions_push_error(const Variant &p_arg1, const Args &...p_args) {
		std::array<Variant, 1 + sizeof...(Args)> variant_args{{ p_arg1, Variant(p_args)... }};
		std::array<const Variant *, 1 + sizeof...(Args)> call_args;
		for (size_t i = 0; i < variant_args.size(); i++) {
			call_args[i] = &variant_args[i];
		}
		Callable::CallError _err;
		VariantUtilityFunctions::push_error(call_args.data(), variant_args.size(), _err);
	}

	template <typename... Args>
	static void UtilityFunctions_push_warning(const Variant &p_arg1, const Args &...p_args) {
		std::array<Variant, 1 + sizeof...(Args)> variant_args{{ p_arg1, Variant(p_args)... }};
		std::array<const Variant *, 1 + sizeof...(Args)> call_args;
		for (size_t i = 0; i < variant_args.size(); i++) {
			call_args[i] = &variant_args[i];
		}
		Callable::CallError _err;
		VariantUtilityFunctions::push_warning(call_args.data(), variant_args.size(), _err);
	}

	template <typename... Args>
	static void UtilityFunctions_print(const Variant &p_arg1, const Args &...p_args) {
		std::array<Variant, 1 + sizeof...(Args)> variant_args{{ p_arg1, Variant(p_args)... }};
		std::array<const Variant *, 1 + sizeof...(Args)> call_args;
		for (size_t i = 0; i < variant_args.size(); i++) {
			call_args[i] = &variant_args[i];
		}
		Callable::CallError _err;
		VariantUtilityFunctions::print(call_args.data(), variant_args.size(), _err);
	}
}
using UtilityFunctions = VariantUtilityFunctions;