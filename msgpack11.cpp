#include "msgpack11.hpp"
#include "..\h\none.h"
#include <array>
#include <cassert>
#include <cmath>
#include <cstdlib>
#include <cstdio>
#include <limits>
#include <deque>
#include <tuple>
#include <algorithm>
#include <functional>
#include <exception>
#include <stdexcept>
#include <type_traits>
#include <typeindex>
#include <utility>
#include <bit>
#include <unordered_map>
#include <map>

namespace msgpack11
{
//	static const int max_depth = 200;
	
	constexpr std::partial_ordering operator<=>(const std::unordered_map<MsgPack,MsgPack>&,const std::unordered_map<MsgPack,MsgPack>&)
	{
		return std::partial_ordering::unordered;
	}
	
	/* Helper for representing null - just a do-nothing struct, plus comparison
 * operators so the helpers in MsgPackValue work. We can't use nullptr_t because
 * it may not be orderable.
 */
	
	/* * * * * * * * * * * * * * * * * * * *
 * MasPackValue
 */
	
	class TypeError : public std::runtime_error
	{
	public:
		TypeError(std::type_index expected,std::type_index got):
			std::runtime_error(std::string()+"expected "+expected.name()+", but got "+got.name()){}
			~TypeError()=default;
	};
	
	class MsgPackValue
	{
	public:
		MsgPack::Type type()                                            const;
		virtual bool operator==(const MsgPackValue &other)              const=0;
		virtual std::partial_ordering operator<=>(const MsgPackValue&)  const=0;
		virtual void dump(std::ostream& os)                             const=0;
		//immutable type specify
		virtual explicit operator MsgPack::float32          ()const;
		virtual explicit operator MsgPack::float64          ()const;
		virtual explicit operator MsgPack::int128           ()const;
		virtual explicit operator MsgPack::int8             ()const;
		virtual explicit operator MsgPack::int16            ()const;
		virtual explicit operator MsgPack::int32            ()const;
		virtual explicit operator MsgPack::int64            ()const;
		virtual explicit operator MsgPack::uint8            ()const;
		virtual explicit operator MsgPack::uint16           ()const;
		virtual explicit operator MsgPack::uint32           ()const;
		virtual explicit operator MsgPack::uint64           ()const;
		virtual explicit operator MsgPack::boolean          ()const;
		virtual explicit operator MsgPack::string    const &()const;
		virtual explicit operator MsgPack::array     const &()const;
		virtual explicit operator MsgPack::binary    const &()const;
		virtual explicit operator MsgPack::object    const &()const;
		virtual explicit operator MsgPack::extension const &()const;
		//mutable type specify
		virtual explicit operator MsgPack::float32    &();
		virtual explicit operator MsgPack::float64    &();
		virtual explicit operator MsgPack::int128     &();
		virtual explicit operator MsgPack::int8       &();
		virtual explicit operator MsgPack::int16      &();
		virtual explicit operator MsgPack::int32      &();
		virtual explicit operator MsgPack::int64      &();
		virtual explicit operator MsgPack::uint8      &();
		virtual explicit operator MsgPack::uint16     &();
		virtual explicit operator MsgPack::uint32     &();
		virtual explicit operator MsgPack::uint64     &();
		virtual explicit operator MsgPack::boolean    &();
		virtual explicit operator MsgPack::string     &();
		virtual explicit operator MsgPack::array      &();
		virtual explicit operator MsgPack::binary     &();
		virtual explicit operator MsgPack::object     &();
		virtual explicit operator MsgPack::extension  &();
		//member accessing
		virtual MsgPack            const &operator[](size_t i)          const;
		virtual MsgPack                  &operator[] (size_t i);
		virtual MsgPack            const &operator[](const MsgPack &key)const;
		virtual MsgPack                  &operator[](const MsgPack &key);
		virtual ~MsgPackValue()=default;
	};
	
	/* * * * * * * * * * * * * * * * * * * *
 * Serialization
 */
	
	namespace
	{
		static const union
		{
			uint16_t dummy;
			uint8_t bytes[2];
		} endian_check_data { 0x0001 };
		static const bool is_big_endian = endian_check_data.bytes[0] == 0x00;
		
		template< typename T > requires std::is_trivially_copyable_v<T>
		void dump_data(const T& value, std::ostream& os)
		{
			union
			{
				T packed;
				std::array<uint8_t, sizeof(T)> bytes;
			} converter;
			converter.packed=value;
			
			int const n = sizeof(T);
			int const offsets[] = {(n-1), 0};
			int const directions[] = {-1, 1};
			uint8_t const off = offsets[static_cast<int>(is_big_endian)];
			int const dir = directions[static_cast<int>(is_big_endian)];
			for(int i = 0; i < n; ++i)
			{
				os.put(converter.bytes[off + dir * i]);
			}
		}
		
		inline void dump(reverSilly::none, std::ostream& os)
		{
			os.put(0xc0);
		}
		
		inline void dump(MsgPack::float32 value, std::ostream& os)
		{
			os.put(0xca);
			dump_data(value, os);
		}
		
		inline void dump(MsgPack::float64 value, std::ostream& os)
		{
			os.put(0xcb);
			dump_data(value, os);
		}
		
		inline void dump(uint8_t value, std::ostream& os)
		{
			if(128 <= value)
			{
				os.put(0xcc);
			}
			os.put(value);
		}
		
		inline void dump(uint16_t value, std::ostream& os)
		{
			if( value < (1<<8) )
			{
				dump(static_cast<uint8_t>(value), os );
			}
			else
			{
				os.put(0xcd);
				dump_data(value, os);
			}
		}
		
		inline void dump(uint32_t value, std::ostream& os)
		{
			if( value < (1 << 16) )
			{
				dump(static_cast<uint16_t>(value), os );
			}
			else
			{
				os.put(0xce);
				dump_data(value, os);
			}
		}
		
		inline void dump(uint64_t value, std::ostream& os)
		{
			if( value < (1ULL << 32) )
			{
				dump(static_cast<uint32_t>(value), os );
			}
			else
			{
				os.put(0xcf);
				dump_data(value, os);
			}
		}
		
		inline void dump(int8_t value, std::ostream& os)
		{
			if( value < -32 )
			{
				os.put(0xd0);
			}
			os.put(value);
		}
		
		inline void dump(int16_t value, std::ostream& os)
		{
			if( value < -(1 << 7) )
			{
				os.put(0xd1);
				dump_data(value, os);
			}
			else if( value <= 0 )
			{
				dump(static_cast<int8_t>(value), os );
			}
			else
			{
				dump(static_cast<uint16_t>(value), os );
			}
		}
		
		inline void dump(int32_t value, std::ostream& os)
		{
			if( value < -(1 << 15) )
			{
				os.put(0xd2);
				dump_data(value, os);
			}
			else if( value <= 0 )
			{
				dump(static_cast<int16_t>(value), os );
			}
			else
			{
				dump(static_cast<uint32_t>(value), os );
			}
		}
		
		inline void dump(int64_t value, std::ostream& os)
		{
			if( value < -(1LL << 31) )
			{
				os.put(0xd3);
				dump_data(value, os);
			}
			else if( value <= 0 )
			{
				dump(static_cast<int32_t>(value), os );
			}
			else
			{
				dump(static_cast<uint64_t>(value), os );
			}
		}
		
		inline void dump(MsgPack::boolean value, std::ostream& os)
		{
			const uint8_t msgpack_value = (value) ? 0xc3 : 0xc2;
			os.put(msgpack_value);
		}
		
		inline void dump(const std::string& value, std::ostream& os)
		{
			size_t const len = value.size();
			if(len <= 0x1f)
			{
				uint8_t const first_byte = 0xa0 | static_cast<uint8_t>(len);
				os.put(first_byte);
			}
			else if(len <= 0xff)
			{
				os.put(0xd9);
				os.put(static_cast<uint8_t>(len));
			}
			else if(len <= 0xffff)
			{
				os.put(0xda);
				dump_data(static_cast<uint16_t>(len), os);
			}
			else if(len <= 0xffffffff)
			{
				os.put(0xdb);
				dump_data(static_cast<uint32_t>(len), os);
			}
			else
			{
				throw std::runtime_error("exceeded maximum data length");
			}
			
			std::for_each(std::begin(value), std::end(value),
				[&os](char v)
				{
					dump_data(v, os);
				});
		}
		
		inline void dump(const MsgPack::array& value, std::ostream& os)
		{
			size_t const len = value.size();
			if(len <= 15)
			{
				uint8_t const first_byte = 0x90 | static_cast<uint8_t>(len);
				os.put(first_byte);
			}
			else if(len <= 0xffff)
			{
				os.put(0xdc);
				dump_data(static_cast<uint16_t>(len), os);
			}
			else if(len <= 0xffffffff)
			{
				os.put(0xdd);
				dump_data(static_cast<uint32_t>(len), os);
			}
			else
			{
				throw std::runtime_error("exceeded maximum data length");
			}
			for(const auto&v:value)
				os<<v;
		}
		
		inline void dump(const MsgPack::object& value, std::ostream& os)
		{
			size_t const len = value.size();
			if(len <= 15)
			{
				uint8_t const first_byte = 0x80 | static_cast<uint8_t>(len);
				os.put(first_byte);
			}
			else if(len <= 0xffff)
			{
				os.put(0xde);
				dump_data(static_cast<uint16_t>(len), os);
			}
			else if(len <= 0xffffffff)
			{
				os.put(0xdf);
				dump_data(static_cast<uint32_t>(len), os);
			}
			else
			{
				throw std::runtime_error("too long value.");
			}
			for(const auto &v:value)
				os<<v.first<<v.second;
		}
		
		inline void dump(const MsgPack::binary& value, std::ostream& os)
		{
			size_t const len = value.size();
			if(len <= 0xff)
			{
				os.put(0xc4);
				dump_data(static_cast<uint8_t>(len), os);
			}
			else if(len <= 0xffff)
			{
				os.put(0xc5);
				dump_data(static_cast<uint16_t>(len), os);
			}
			else if(len <= 0xffffffff)
			{
				os.put(0xc6);
				dump_data(static_cast<uint32_t>(len), os);
			}
			else
			{
				throw std::runtime_error("exceeded maximum data length");
			}
			os.write(reinterpret_cast<const char*>(value.data()), value.size());
		}
		
		inline void dump(const MsgPack::extension& value, std::ostream& os)
		{
			const uint8_t type(std::get<0>(value));
			const MsgPack::binary& data(std::get<1>(value));
			const size_t len = data.size();
			
			if(len == 0x01) {
				os.put(0xd4);
			}
			else if(len == 0x02) {
				os.put(0xd5);
			}
			else if(len == 0x04) {
				os.put(0xd6);
			}
			else if(len == 0x08) {
				os.put(0xd7);
			}
			else if(len == 0x10) {
				os.put(0xd8);
			}
			else if(len <= 0xff) {
				os.put(0xc7);
				os.put(static_cast<uint8_t>(len));
			}
			else if(len <= 0xffff) {
				os.put(0xc8);
				dump_data(static_cast<uint16_t>(len), os);
			}
			else if(len <= 0xffffffff) {
				os.put(0xc9);
				dump_data(static_cast<uint32_t>(len), os);
			}
			else {
				throw std::runtime_error("exceeded maximum data length");
			}
			
			os.put(type);
			os.write(reinterpret_cast<const char*>(data.data()), data.size());
		}
	}
	
	std::ostream& operator<<(std::ostream& os, const MsgPack& msgpack)
	{
		msgpack.m_ptr->dump(os);
		return os;
	}
	
	/* * * * * * * * * * * * * * * * * * * *
 * Value wrappers
 */
	
	template <typename T>
	class Value : public MsgPackValue
	{
	public:
		// Constructors
		Value(T value) requires(std::is_fundamental_v<T>):m_value(value){}
		Value(const T& value) requires(std::is_class_v<T>):m_value(value){}
		Value(T&& value) requires(std::is_class_v<T>):m_value(std::move(value)){}
		Value():m_value(){}
		// Comparisons
		virtual bool operator==(const MsgPackValue &other) const override
		{
			return (type()==other.type())&&(m_value==static_cast<const Value<T>&>(other).m_value);
		}
		virtual std::partial_ordering operator<=>(const MsgPackValue &other) const override
		{
			if(type()<=>other.type()!=0)
				return type()<=>other.type();
			else
			{
				if(type()==other.type())
					return m_value<=>static_cast<const Value<T>&>(other).m_value;
				else
					return std::partial_ordering::unordered;
			}
		}
		T m_value;
		virtual void dump(std::ostream& os) const override { msgpack11::dump(m_value, os); }
		virtual explicit operator T&(){return m_value;}
	};
	
	template<typename T> requires(std::is_fundamental_v<T>)
	class Number final: public Value<T>
	{
	public:
		Number(T value):Value<T>(value){}
		
		bool operator ==(const MsgPackValue &other) const override
		{
			switch( other.type() )
			{
				case MsgPack::Type::FLOAT32 : // fall through
				case MsgPack::Type::FLOAT64 : // fall through
				{
					return operator MsgPack::float64()==other.operator MsgPack::float64();
				} break;
				case MsgPack::Type::UINT8   : // fall through
				case MsgPack::Type::UINT16  : // fall through
				case MsgPack::Type::UINT32  : // fall through
				case MsgPack::Type::UINT64  : // fall through
				case MsgPack::Type::INT8    : // fall through
				case MsgPack::Type::INT16   : // fall through
				case MsgPack::Type::INT32   : // fall through
				case MsgPack::Type::INT64   : // fall through
				{
					return operator MsgPack::int128()==other.operator MsgPack::int128();
				} break;
				default:
					{
						return Value<T>::operator ==(other);
					} break;
			}
		}
		
		std::partial_ordering operator<=>(const MsgPackValue &other) const override
		{
			switch( other.type() )
			{
				case MsgPack::Type::FLOAT32 : // fall through
				case MsgPack::Type::FLOAT64 : // fall through
				{
					return operator MsgPack::float64()<=>other.operator MsgPack::float64();
				} break;
				case MsgPack::Type::UINT8   : // fall through
				case MsgPack::Type::UINT16  : // fall through
				case MsgPack::Type::UINT32  : // fall through
				case MsgPack::Type::UINT64  : // fall through
				case MsgPack::Type::INT8    : // fall through
				case MsgPack::Type::INT16   : // fall through
				case MsgPack::Type::INT32   : // fall through
				case MsgPack::Type::INT64   : // fall through
				{
					return operator MsgPack::int128()<=>other.operator MsgPack::int128();
				} break;
				default:
					{
						return Value<T>::operator<=>(other);
					} break;
			}
		}
		virtual explicit operator MsgPack::float32   ()const override{return static_cast<MsgPack::float32>(Value<T>::m_value);}
		virtual explicit operator MsgPack::float64   ()const override{return static_cast<MsgPack::float64>(Value<T>::m_value);}
		virtual explicit operator MsgPack::int8      ()const override{return static_cast<MsgPack::int8>   (Value<T>::m_value);}
		virtual explicit operator MsgPack::int16     ()const override{return static_cast<MsgPack::int16>  (Value<T>::m_value);}
		virtual explicit operator MsgPack::int32     ()const override{return static_cast<MsgPack::int32>  (Value<T>::m_value);}
		virtual explicit operator MsgPack::int64     ()const override{return static_cast<MsgPack::int64>  (Value<T>::m_value);}
		virtual explicit operator MsgPack::uint8     ()const override{return static_cast<MsgPack::uint8>  (Value<T>::m_value);}
		virtual explicit operator MsgPack::uint16    ()const override{return static_cast<MsgPack::uint16> (Value<T>::m_value);}
		virtual explicit operator MsgPack::uint32    ()const override{return static_cast<MsgPack::uint32> (Value<T>::m_value);}
		virtual explicit operator MsgPack::uint64    ()const override{return static_cast<MsgPack::uint64> (Value<T>::m_value);}
		virtual explicit operator MsgPack::int128    ()const override{return static_cast<MsgPack::int128> (Value<T>::m_value);}
		virtual explicit operator MsgPack::boolean   ()const override{return static_cast<MsgPack::boolean>(Value<T>::m_value);}
	};
	
	template class Number<MsgPack::float32>;
	template class Number<MsgPack::float64>;
	template class Number<MsgPack::int8>;
	template class Number<MsgPack::int16>;
	template class Number<MsgPack::int32>;
	template class Number<MsgPack::int64>;
	template class Number<MsgPack::uint8>;
	template class Number<MsgPack::uint16>;
	template class Number<MsgPack::uint32>;
	template class Number<MsgPack::uint64>;
	template class Number<MsgPack::boolean>;
	
	template<typename T> requires(std::is_class_v<T>)
	class Compound final: public Value<T>
	{
	public:
		virtual operator const T&() const override{return Value<T>::m_value;}
		Compound(const T& thing):Value<T>(thing){}
		Compound(T&& thing):Value<T>(thing){}
		
		const MsgPack & operator[](size_t i) const override
		{
			if constexpr(std::is_same_v<T,MsgPack::array>)
				return Value<T>::m_value.at(i);
			else
				throw TypeError(typeid(MsgPack::array),typeid(T));
		}
		MsgPack & operator[](size_t i) override
		{
			if constexpr(std::is_same_v<T,MsgPack::array>)
				return Value<T>::m_value.at(i);
			else
				throw TypeError(typeid(MsgPack::array),typeid(T));
		}
		
		MsgPack const &operator[](const MsgPack &key) const override
		{
			if constexpr(std::is_same_v<T,MsgPack::object>)
				return Value<T>::m_value.at(key);
			else
				throw TypeError(typeid(MsgPack::object),typeid(T));
		}
		MsgPack& operator[](const MsgPack &key) override
		{
			if constexpr(std::is_same_v<T,MsgPack::object>)
				return Value<T>::m_value[key];
			else
				throw TypeError(typeid(MsgPack::object),typeid(T));
		}
	};
	template class Compound<MsgPack::string>;
	template class Compound<MsgPack::array>;
	template class Compound<MsgPack::binary>;
	template class Compound<MsgPack::object>;
	template class Compound<MsgPack::extension>;
	
	template class Value<reverSilly::none>;
	
	MsgPack::Type MsgPackValue::type()const
	{
		static const std::unordered_map<std::type_index,MsgPack::Type>table
		{
			{typeid(Number<MsgPack::float32>),MsgPack::Type::FLOAT32},
			{typeid(Number<MsgPack::float64>),MsgPack::Type::FLOAT64},
			{typeid(Number<MsgPack::int8>),MsgPack::Type::INT8},
			{typeid(Number<MsgPack::int16>),MsgPack::Type::INT16},
			{typeid(Number<MsgPack::int32>),MsgPack::Type::INT32},
			{typeid(Number<MsgPack::int64>),MsgPack::Type::INT64},
			{typeid(Number<MsgPack::uint8>),MsgPack::Type::UINT8},
			{typeid(Number<MsgPack::uint16>),MsgPack::Type::UINT16},
			{typeid(Number<MsgPack::uint32>),MsgPack::Type::UINT32},
			{typeid(Number<MsgPack::uint64>),MsgPack::Type::UINT64},
			{typeid(Number<MsgPack::boolean>),MsgPack::Type::BOOL},
			{typeid(Compound<MsgPack::array>),MsgPack::Type::ARRAY},
			{typeid(Compound<MsgPack::extension>),MsgPack::Type::EXTENSION},
			{typeid(Compound<MsgPack::object>),MsgPack::Type::OBJECT},
			{typeid(Compound<MsgPack::binary>),MsgPack::Type::BINARY},
			{typeid(Compound<MsgPack::string>),MsgPack::Type::STRING},
			{typeid(Value<reverSilly::none>),MsgPack::Type::NUL}
		};
		return table.at(typeid(*this));
	}
	/* * * * * * * * * * * * * * * * * * * *
 * Constructors
 */
	
	MsgPack::MsgPack()                                 : m_ptr(std::make_shared<Value<reverSilly::none>>()){}
	MsgPack::MsgPack(std::nullptr_t)                   : m_ptr(std::make_shared<Value<reverSilly::none>>()){}
//	
//	template<typename T> requires(std::is_fundamental_v<T>)
//	MsgPack::MsgPack(T value):m_ptr(std::make_shared<Number<T>>(value)){}
//	
//	template MsgPack::MsgPack(MsgPack::float32);
//	template MsgPack::MsgPack(MsgPack::float64);
//	template MsgPack::MsgPack(MsgPack::int8);
//	template MsgPack::MsgPack(MsgPack::int16);
//	template MsgPack::MsgPack(MsgPack::int32);
//	template MsgPack::MsgPack(MsgPack::int64);
//	template MsgPack::MsgPack(MsgPack::uint8);
//	template MsgPack::MsgPack(MsgPack::uint16);
//	template MsgPack::MsgPack(MsgPack::uint32);
//	template MsgPack::MsgPack(MsgPack::uint64);
//	template MsgPack::MsgPack(MsgPack::boolean);
//	
//	template<typename T> requires(std::is_class_v<T>)
//	MsgPack::MsgPack(const T& value):m_ptr(std::make_shared<Compound<T>>(value)){}
//	template<typename T> requires(std::is_class_v<T>)
//	MsgPack::MsgPack(T&& value):m_ptr(std::make_shared<Compound<T>>(value)){}
//	
//	template MsgPack::MsgPack(MsgPack::array&&);
//	template MsgPack::MsgPack(MsgPack::array const&);
//	template MsgPack::MsgPack(MsgPack::object&&);
//	template MsgPack::MsgPack(MsgPack::object const&);
//	template MsgPack::MsgPack(MsgPack::extension&&);
//	template MsgPack::MsgPack(MsgPack::extension const&);
//	template MsgPack::MsgPack(MsgPack::binary&&);
//	template MsgPack::MsgPack(MsgPack::binary const&);
//	template MsgPack::MsgPack(MsgPack::string&&);
//	template MsgPack::MsgPack(MsgPack::string const&);

	MsgPack::MsgPack(MsgPack::float32 value)           : m_ptr(std::make_shared<Number<MsgPack::float32>>(value)) {}
	MsgPack::MsgPack(MsgPack::float64 value)           : m_ptr(std::make_shared<Number<MsgPack::float64>>(value)) {}
	MsgPack::MsgPack(MsgPack::int8 value)              : m_ptr(std::make_shared<Number<MsgPack::int8>>(value)) {}
	MsgPack::MsgPack(MsgPack::int16 value)             : m_ptr(std::make_shared<Number<MsgPack::int16>>(value)) {}
	MsgPack::MsgPack(MsgPack::int32 value)             : m_ptr(std::make_shared<Number<MsgPack::int32>>(value)) {}
	MsgPack::MsgPack(MsgPack::int64 value)             : m_ptr(std::make_shared<Number<MsgPack::int64>>(value)) {}
	MsgPack::MsgPack(MsgPack::uint8 value)             : m_ptr(std::make_shared<Number<MsgPack::uint8>>(value)) {}
	MsgPack::MsgPack(MsgPack::uint16 value)            : m_ptr(std::make_shared<Number<MsgPack::uint16>>(value)) {}
	MsgPack::MsgPack(MsgPack::uint32 value)            : m_ptr(std::make_shared<Number<MsgPack::uint32>>(value)) {}
	MsgPack::MsgPack(MsgPack::uint64 value)            : m_ptr(std::make_shared<Number<MsgPack::uint64>>(value)) {}
	MsgPack::MsgPack(MsgPack::boolean value)           : m_ptr(std::make_shared<Number<MsgPack::boolean>>(value)) {}
	MsgPack::MsgPack(const MsgPack::string &value)     : m_ptr(std::make_shared<Compound<MsgPack::string>>(value)) {}
	MsgPack::MsgPack(MsgPack::string &&value)          : m_ptr(std::make_shared<Compound<MsgPack::string>>(move(value))) {}
	MsgPack::MsgPack(const MsgPack::array &values)     : m_ptr(std::make_shared<Compound<MsgPack::array>>(values)) {}
	MsgPack::MsgPack(MsgPack::array &&values)          : m_ptr(std::make_shared<Compound<MsgPack::array>>(move(values))) {}
	MsgPack::MsgPack(const MsgPack::object &values)    : m_ptr(std::make_shared<Compound<MsgPack::object>>(values)) {}
	MsgPack::MsgPack(MsgPack::object &&values)         : m_ptr(std::make_shared<Compound<MsgPack::object>>(move(values))) {}
	MsgPack::MsgPack(const MsgPack::binary &values)    : m_ptr(std::make_shared<Compound<MsgPack::binary>>(values)) {}
	MsgPack::MsgPack(MsgPack::binary &&values)         : m_ptr(std::make_shared<Compound<MsgPack::binary>>(move(values))) {}
	MsgPack::MsgPack(const MsgPack::extension &values) : m_ptr(std::make_shared<Compound<MsgPack::extension>>(values)) {}
	MsgPack::MsgPack(MsgPack::extension &&values)      : m_ptr(std::make_shared<Compound<MsgPack::extension>>(move(values))) {}
	
	/* * * * * * * * * * * * * * * * * * * *
 * Accessors
 */
	
	MsgPack::Type MsgPack::type() const{ return m_ptr->type(); }
	//immutable type specify
	template<typename T> requires(std::is_fundamental_v<T>)
	MsgPack::operator T() const{return m_ptr->operator T();}
	template<typename T> requires(!std::is_fundamental_v<T>)
	MsgPack::operator const T&() const{return m_ptr->operator const T&();}
	//mutable ones
	template<typename T> requires(!std::is_const_v<T>)
	MsgPack::operator T&(){return m_ptr->operator T&();}
	
	template MsgPack::operator MsgPack::int8() const;
	template MsgPack::operator MsgPack::int16() const;
	template MsgPack::operator MsgPack::int32() const;
	template MsgPack::operator MsgPack::int64() const;
	template MsgPack::operator MsgPack::uint8() const;
	template MsgPack::operator MsgPack::uint16() const;
	template MsgPack::operator MsgPack::uint32() const;
	template MsgPack::operator MsgPack::uint64() const;
	template MsgPack::operator MsgPack::int128() const;
	template MsgPack::operator MsgPack::float32() const;
	template MsgPack::operator MsgPack::float64() const;
	template MsgPack::operator MsgPack::boolean() const;
	template MsgPack::operator const MsgPack::string&() const;
	template MsgPack::operator const MsgPack::array&() const;
	template MsgPack::operator const MsgPack::object&() const;
	template MsgPack::operator const MsgPack::binary&() const;
	template MsgPack::operator const MsgPack::extension&() const;
	
	template MsgPack::operator MsgPack::int8&();
	template MsgPack::operator MsgPack::int16&();
	template MsgPack::operator MsgPack::int32&();
	template MsgPack::operator MsgPack::int64&();
	template MsgPack::operator MsgPack::uint8&();
	template MsgPack::operator MsgPack::uint16&();
	template MsgPack::operator MsgPack::uint32&();
	template MsgPack::operator MsgPack::uint64&();
	template MsgPack::operator MsgPack::int128&();
	template MsgPack::operator MsgPack::float32&();
	template MsgPack::operator MsgPack::float64&();
	template MsgPack::operator MsgPack::boolean&();
	template MsgPack::operator MsgPack::string&();
	template MsgPack::operator MsgPack::array&();
	template MsgPack::operator MsgPack::object&();
	template MsgPack::operator MsgPack::binary&();
	template MsgPack::operator MsgPack::extension&();
	
	const MsgPack &MsgPack::operator[] (size_t i)                     const { return m_ptr->operator[](i); }
	MsgPack &MsgPack::operator[] (size_t i)                                 { return m_ptr->operator[](i); }
	const MsgPack &MsgPack::operator[] (const MsgPack &key)           const { return m_ptr->operator[](key); }
	MsgPack &MsgPack::operator[] (const MsgPack &key)                       { return m_ptr->operator[](key); }
	
	//immutable
	MsgPackValue::operator MsgPack::float32             ()   const { throw TypeError(typeid(Number<MsgPack::float32>),typeid(*this)); }
	MsgPackValue::operator MsgPack::float64             ()   const { throw TypeError(typeid(Number<MsgPack::float64>),typeid(*this)); }
	MsgPackValue::operator MsgPack::int128              ()   const { throw TypeError(typeid(Number<MsgPack::int128>),typeid(*this)); }
	MsgPackValue::operator MsgPack::int8                ()   const { throw TypeError(typeid(Number<MsgPack::int8>),typeid(*this)); }
	MsgPackValue::operator MsgPack::int16               ()   const { throw TypeError(typeid(Number<MsgPack::int16>),typeid(*this)); }
	MsgPackValue::operator MsgPack::int32               ()   const { throw TypeError(typeid(Number<MsgPack::int32>),typeid(*this)); }
	MsgPackValue::operator MsgPack::int64               ()   const { throw TypeError(typeid(Number<MsgPack::int64>),typeid(*this)); }
	MsgPackValue::operator MsgPack::uint8               ()   const { throw TypeError(typeid(Number<MsgPack::uint8>),typeid(*this)); }
	MsgPackValue::operator MsgPack::uint16              ()   const { throw TypeError(typeid(Number<MsgPack::uint16>),typeid(*this)); }
	MsgPackValue::operator MsgPack::uint32              ()   const { throw TypeError(typeid(Number<MsgPack::uint32>),typeid(*this)); }
	MsgPackValue::operator MsgPack::uint64              ()   const { throw TypeError(typeid(Number<MsgPack::uint64>),typeid(*this)); }
	MsgPackValue::operator MsgPack::boolean             ()   const { throw TypeError(typeid(Number<MsgPack::boolean>),typeid(*this)); }
	MsgPackValue::operator MsgPack::string       const &()   const { throw TypeError(typeid(Compound<MsgPack::string>),typeid(*this)); }
	MsgPackValue::operator MsgPack::array        const &()   const { throw TypeError(typeid(Compound<MsgPack::array>),typeid(*this)); }
	MsgPackValue::operator MsgPack::object       const &()   const { throw TypeError(typeid(Compound<MsgPack::object>),typeid(*this)); }
	MsgPackValue::operator MsgPack::binary       const &()   const { throw TypeError(typeid(Compound<MsgPack::binary>),typeid(*this)); }
	MsgPackValue::operator MsgPack::extension    const &()   const { throw TypeError(typeid(Compound<MsgPack::extension>),typeid(*this)); }
	//mutable
	MsgPackValue::operator MsgPack::float32  &()         { throw TypeError(typeid(Number<MsgPack::float32>),typeid(*this)); }
	MsgPackValue::operator MsgPack::float64  &()         { throw TypeError(typeid(Number<MsgPack::float64>),typeid(*this)); }
	MsgPackValue::operator MsgPack::int128   &()         { throw TypeError(typeid(Number<MsgPack::int128>),typeid(*this)); }
	MsgPackValue::operator MsgPack::int8     &()         { throw TypeError(typeid(Number<MsgPack::int8>),typeid(*this)); }
	MsgPackValue::operator MsgPack::int16    &()         { throw TypeError(typeid(Number<MsgPack::int16>),typeid(*this)); }
	MsgPackValue::operator MsgPack::int32    &()         { throw TypeError(typeid(Number<MsgPack::int32>),typeid(*this)); }
	MsgPackValue::operator MsgPack::int64    &()         { throw TypeError(typeid(Number<MsgPack::int64>),typeid(*this)); }
	MsgPackValue::operator MsgPack::uint8    &()         { throw TypeError(typeid(Number<MsgPack::uint8>),typeid(*this)); }
	MsgPackValue::operator MsgPack::uint16   &()         { throw TypeError(typeid(Number<MsgPack::uint16>),typeid(*this)); }
	MsgPackValue::operator MsgPack::uint32   &()         { throw TypeError(typeid(Number<MsgPack::uint32>),typeid(*this)); }
	MsgPackValue::operator MsgPack::uint64   &()         { throw TypeError(typeid(Number<MsgPack::uint64>),typeid(*this)); }
	MsgPackValue::operator MsgPack::boolean  &()         { throw TypeError(typeid(Number<MsgPack::boolean>),typeid(*this)); }
	MsgPackValue::operator MsgPack::string   &()         { throw TypeError(typeid(Compound<MsgPack::string>),typeid(*this)); }
	MsgPackValue::operator MsgPack::array    &()         { throw TypeError(typeid(Compound<MsgPack::array>),typeid(*this)); }
	MsgPackValue::operator MsgPack::object   &()         { throw TypeError(typeid(Compound<MsgPack::object>),typeid(*this)); }
	MsgPackValue::operator MsgPack::binary   &()         { throw TypeError(typeid(Compound<MsgPack::binary>),typeid(*this)); }
	MsgPackValue::operator MsgPack::extension&()         { throw TypeError(typeid(Compound<MsgPack::extension>),typeid(*this)); }
	//access
	const MsgPack &MsgPackValue::operator[](size_t)         const { throw TypeError(typeid(Compound<MsgPack::array>),typeid(*this)); }
	MsgPack       &MsgPackValue::operator[](size_t)               { throw TypeError(typeid(Compound<MsgPack::array>),typeid(*this)); }
	const MsgPack &MsgPackValue::operator[](const MsgPack&) const { throw TypeError(typeid(Compound<MsgPack::object>),typeid(*this)); }
	MsgPack       &MsgPackValue::operator[](const MsgPack&)       { throw TypeError(typeid(Compound<MsgPack::object>),typeid(*this)); }
	
	/* * * * * * * * * * * * * * * * * * * *
 * Comparison
 */
	
	bool MsgPack::operator==(const MsgPack &other) const{return *m_ptr==*other.m_ptr;}
	
	std::partial_ordering MsgPack::operator<=> (const MsgPack &other) const{return *m_ptr<=>*other.m_ptr;}
	
	namespace
	{
		/* MsgPackParser
 *
 * Object that tracks all state of an in-progress parse.
 */
		namespace MsgPackParser
		{
			MsgPack parse_msgpack(std::istream& is, int depth);
			
			template< typename T >
			void read_bytes(std::istream& is, T& bytes)
			{
				static_assert(std::is_trivially_copyable_v<T>,"byte read not guaranteed for non-primitive types");
				int n = sizeof(T);
				
				int const offsets[]{(n-1), 0};
				int const directions[]{-1, 1};
				
				uint8_t* dst_ptr = reinterpret_cast<uint8_t*>(&bytes) + offsets[static_cast<int>(is_big_endian)];
				int const dir = directions[static_cast<int>(is_big_endian)];
				for(int i = 0; i < n; ++i)
				{
					*dst_ptr = is.get();
					dst_ptr += dir;
				}
				// NB: if the read fails it's prefered to return 0 rather than
				//      corrupted value, for example in the case of reading data size.
				if (is.fail() || is.eof())
				{
					bytes = 0;
				}
			}
			
			/* fail(msg, err_ret = MsgPack())
     *
     * Mark this parse as m_failed.
     */
			MsgPack fail(std::istream& is)
			{
				is.setstate(std::ios::failbit);
				return MsgPack();
			}
			
			MsgPack parse_invalid(std::istream& is, uint8_t,size_t)
			{
				return fail(is);
			}
			
			MsgPack parse_nil(std::istream&, uint8_t,size_t)
			{
				return MsgPack();
			}
			
			MsgPack parse_bool(std::istream&, uint8_t first_byte,size_t)
			{
				return MsgPack(first_byte==0xc3);
			}
			
			template< typename T >
			MsgPack parse_arith(std::istream& is, uint8_t,size_t)
			{
				T tmp;
				read_bytes(is, tmp);
				return MsgPack(tmp);
			}
			
			inline std::string parse_string_impl(std::istream& is, uint32_t bytes)
			{
				std::string ret;
				ret.resize(bytes);
				is.read(&ret[0], bytes);
				return ret;
			}
			
			template< typename T >
			MsgPack parse_string(std::istream& is, uint8_t, size_t)
			{
				T bytes;
				read_bytes(is, bytes);
				return MsgPack(parse_string_impl(is, static_cast<uint32_t>(bytes)));
			}
			
			MsgPack::array parse_array_impl(std::istream& is, uint32_t bytes,size_t depth)
			{
				MsgPack::array res;
//				res.reserve(bytes);
				
				for(uint32_t i = 0; i < bytes; ++i)
				{
					res.push_back(parse_msgpack(is, depth));
				}
				return res;
			}
			
			template< typename T >
			MsgPack parse_array(std::istream& is, uint8_t, size_t depth)
			{
				T bytes;
				read_bytes(is, bytes);
				return MsgPack(parse_array_impl(is, static_cast<uint32_t>(bytes), depth));
			}
			
			MsgPack::object parse_object_impl(std::istream& is, uint32_t bytes,size_t depth)
			{
				MsgPack::object res;
				
				for(uint32_t i = 0; i < bytes; ++i)
				{
					MsgPack key=parse_msgpack(is, depth);
					MsgPack value=parse_msgpack(is, depth);
					res.insert(std::make_pair(std::move(key), std::move(value)));
				}
				return res;
			}
			
			template< typename T >
			MsgPack parse_object(std::istream& is, uint8_t,size_t depth)
			{
				T bytes;
				read_bytes(is, bytes);
				return MsgPack(parse_object_impl(is, static_cast<uint32_t>(bytes), depth));
			}
			
			MsgPack::binary parse_binary_impl(std::istream& is, uint32_t bytes)
			{
				MsgPack::binary ret;
				ret.resize(bytes);
				is.read(reinterpret_cast<char*>(ret.data()),bytes);
				return ret;
			}
			
			template< typename T >
			MsgPack parse_binary(std::istream& is, uint8_t,size_t)
			{
				T bytes;
				read_bytes(is,bytes);
				return MsgPack(parse_binary_impl(is,static_cast<uint32_t>(bytes)));
			}
			
			template< typename T >
			MsgPack parse_extension(std::istream& is, uint8_t,size_t)
			{
				T bytes;
				read_bytes(is, bytes);
				uint8_t type;
				read_bytes(is, type);
				const MsgPack::binary data=parse_binary_impl(is, static_cast<uint32_t>(bytes));
				return MsgPack(std::make_tuple(type, std::move(data)));
			}
			
			MsgPack parse_pos_fixint(std::istream&, uint8_t first_byte, size_t)
			{
				return MsgPack( first_byte );
			}
			
			MsgPack parse_fixobject(std::istream& is, uint8_t first_byte,size_t depth)
			{
				uint32_t const bytes = first_byte & 0x0f;
				return MsgPack(parse_object_impl(is, bytes, depth));
			}
			
			MsgPack parse_fixarray(std::istream& is, uint8_t first_byte,size_t depth)
			{
				uint32_t const bytes = first_byte & 0x0f;
				return MsgPack(parse_array_impl(is, bytes, depth));
			}
			
			MsgPack parse_fixstring(std::istream& is, uint8_t first_byte,size_t)
			{
				uint32_t const bytes = first_byte & 0x1f;
				return MsgPack(parse_string_impl(is, bytes));
			}
			
			MsgPack parse_neg_fixint(std::istream&, uint8_t first_byte, size_t)
			{
				return MsgPack(*reinterpret_cast<int8_t*>(&first_byte));
			}
			
			MsgPack parse_fixext(std::istream& is, uint8_t first_byte, size_t)
			{
				uint8_t type;
				read_bytes(is, type);
				uint32_t const BYTES = 1 << (first_byte - 0xd4u);
				const MsgPack::binary data = parse_binary_impl(is, BYTES);
				return MsgPack(std::make_tuple(type, std::move(data)));
			}
			
			/* parse_msgpack()
     *
     * Parse a JSON object.
     */
			MsgPack parse_msgpack(std::istream& is, int depth)
			{
				static const std::array<std::function<MsgPack(std::istream&,uint8_t,size_t)>,256>parsers{[]()
				{
					using parser_map_type=std::pair<uint8_t,std::function<MsgPack(std::istream&,uint8_t,size_t)>>;
					std::array<parser_map_type,36> const parser_template
					{{
						parser_map_type{0x7fu,MsgPackParser::parse_pos_fixint},
						parser_map_type{0x8fu,MsgPackParser::parse_fixobject},
						parser_map_type{0x9fu,MsgPackParser::parse_fixarray},
						parser_map_type{0xbfu,MsgPackParser::parse_fixstring},
						parser_map_type{0xc0u,MsgPackParser::parse_nil},
						parser_map_type{0xc1u,MsgPackParser::parse_invalid},
						parser_map_type{0xc3u,MsgPackParser::parse_bool},
						parser_map_type{0xc4u,MsgPackParser::parse_binary<uint8_t>},
						parser_map_type{0xc5u,MsgPackParser::parse_binary<uint16_t>},
						parser_map_type{0xc6u,MsgPackParser::parse_binary<uint32_t>},
						parser_map_type{0xc7u,MsgPackParser::parse_extension<uint8_t>},
						parser_map_type{0xc8u,MsgPackParser::parse_extension<uint16_t>},
						parser_map_type{0xc9u,MsgPackParser::parse_extension<uint32_t>},
						parser_map_type{0xcau,MsgPackParser::parse_arith<float>},
						parser_map_type{0xcbu,MsgPackParser::parse_arith<double>},
						parser_map_type{0xccu,MsgPackParser::parse_arith<uint8_t>},
						parser_map_type{0xcdu,MsgPackParser::parse_arith<uint16_t>},
						parser_map_type{0xceu,MsgPackParser::parse_arith<uint32_t>},
						parser_map_type{0xcfu,MsgPackParser::parse_arith<uint64_t>},
						parser_map_type{0xd0u,MsgPackParser::parse_arith<int8_t>},
						parser_map_type{0xd1u,MsgPackParser::parse_arith<int16_t>},
						parser_map_type{0xd2u,MsgPackParser::parse_arith<int32_t>},
						parser_map_type{0xd3u,MsgPackParser::parse_arith<int64_t>},
						parser_map_type{0xd8u,MsgPackParser::parse_fixext},
						parser_map_type{0xd9u,MsgPackParser::parse_string<uint8_t>},
						parser_map_type{0xdau,MsgPackParser::parse_string<uint16_t>},
						parser_map_type{0xdbu,MsgPackParser::parse_string<uint32_t>},
						parser_map_type{0xdcu,MsgPackParser::parse_array<uint16_t>},
						parser_map_type{0xddu,MsgPackParser::parse_array<uint32_t>},
						parser_map_type{0xdeu,MsgPackParser::parse_object<uint16_t>},
						parser_map_type{0xdfu,MsgPackParser::parse_object<uint32_t>},
						parser_map_type{0xffu,MsgPackParser::parse_neg_fixint}
					}};
					
					std::array<std::function<MsgPack(std::istream&,uint8_t,size_t)>,256> parsers;
					int i=0;
					for(const auto &parser:parser_template)
						for(;i<=parser.first;i++)
							parsers[i]=parser.second;
					return parsers;
				}()};
				auto const first_byte{is.get()};
				// check for fail/eof after get() as eof only set after read past the end
				if (is.fail() || is.eof())
				{
					return fail(is);
				}
				
				MsgPack ret=parsers.at(first_byte)(is,first_byte,depth+1);
				
				if (is.fail() || is.eof())
				{
					return fail(is);
				}
				return ret;
			}
		};
		
	}//namespace {
	
	std::istream& operator>>(std::istream& is, MsgPack& msgpack)
	{
		msgpack=MsgPackParser::parse_msgpack(is,0);
		return is;
	}
	
	MsgPack MsgPack::parse(std::istream& is)
	{
		return MsgPackParser::parse_msgpack(is,0);
	}
	
	MsgPack MsgPack::parse(std::istream& is, std::string &err)
	{
		MsgPack ret = MsgPack::parse(is);
		if (is.eof())
		{
			err = "end of buffer.";
		}
		else if (is.fail())
		{
			err = "format error.";
		}
		return ret;
	}
	
	MsgPack MsgPack::parse(const std::string &in,std::string &err)
	{
		std::stringstream ss(in);
		return MsgPack::parse(ss, err);
	}
	
	// Documented in msgpack.hpp
	std::vector<MsgPack> MsgPack::parse_multi(const std::string &in,
		std::string::size_type &parser_stop_pos,
		std::string &err)
	{
		std::stringstream ss(in);
		
		std::vector<MsgPack> msgpack_vec;
		while (static_cast<size_t>(ss.tellg()) != in.size() && !ss.eof() && !ss.fail())
		{
			auto next(MsgPack::parse(ss, err));
			if (!ss.fail())
			{
				msgpack_vec.emplace_back(std::move(next));
				parser_stop_pos = ss.tellg();
			}
		}
		
		return msgpack_vec;
	}
	
	/* * * * * * * * * * * * * * * * * * * *
 * Shape-checking
 */
	
	bool MsgPack::has_shape(const shape & types,std::string &err) const
	{
		if (!is_object())
		{
			err="expected MessagePack object";
			return false;
		}
		
		for (auto & item : types)
		{
			if ((*this)[item.first].type() != item.second)
			{
				err="bad type for " + item.first;
				return false;
			}
		}
		
		return true;
	}
	
} // namespace msgpack11
size_t std::hash<msgpack11::MsgPack>::operator()(const msgpack11::MsgPack &thing) const noexcept
{
	switch (thing.type())
	{
	case msgpack11::MsgPack::Type::FLOAT32:
	case msgpack11::MsgPack::Type::FLOAT64:
		return std::bit_cast<size_t>(thing.m_ptr->operator ::msgpack11::MsgPack::float64());
	case msgpack11::MsgPack::Type::NUMBER:
	case msgpack11::MsgPack::Type::INT:
	case msgpack11::MsgPack::Type::INT8:
	case msgpack11::MsgPack::Type::INT16:   
	case msgpack11::MsgPack::Type::INT32:
	case msgpack11::MsgPack::Type::INT64:    
	case msgpack11::MsgPack::Type::UINT8:  
	case msgpack11::MsgPack::Type::UINT16:  
	case msgpack11::MsgPack::Type::UINT32:
	case msgpack11::MsgPack::Type::UINT64:  
	case msgpack11::MsgPack::Type::BOOL:
		return thing.m_ptr->operator ::msgpack11::MsgPack::uint64();
	case msgpack11::MsgPack::Type::STRING:
		return std::hash<std::string>()(thing.m_ptr->operator const ::msgpack11::MsgPack::string&());
	case msgpack11::MsgPack::Type::BINARY:
	case msgpack11::MsgPack::Type::ARRAY:  
	case msgpack11::MsgPack::Type::OBJECT:
	case msgpack11::MsgPack::Type::EXTENSION:
	default:
		return reinterpret_cast<size_t>(thing.m_ptr.get());
	case msgpack11::MsgPack::Type::NUL:
		return 0;
	}
}
