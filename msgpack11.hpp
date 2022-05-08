#pragma once

#include <string>
#include <deque>
#include <vector>
#include <tuple>
#include <unordered_map>
#include <memory>
#include <initializer_list>
#include <istream>
#include <ostream>
#include <sstream>
#include <concepts>

#ifdef _MSC_VER
#if _MSC_VER <= 1800 // VS 2013
#ifndef noexcept
#define noexcept throw()
#endif

#ifndef snprintf
#define snprintf _snprintf_s
#endif
#endif
#endif
namespace msgpack11
{
	class MsgPack;
}
namespace std
{
	template<>
	struct hash<msgpack11::MsgPack>
	{
		size_t operator()(const msgpack11::MsgPack&) const noexcept;
	};
}
namespace msgpack11
{
	class MsgPackValue;
	
	class MsgPack final
	{
	public:
		// Types
		// NB: all ints are numbers but not all numbers are ints.
		enum class Type : uint8_t
		{
			NUMBER      = 1,
			INT         = 2 | NUMBER,
			NUL         = 1 << 2,
			FLOAT32     = 2 << 2 | NUMBER,
			FLOAT64     = 3 << 2 | NUMBER,
			INT8        = 4 << 2 | INT,
			INT16       = 5 << 2 | INT,
			INT32       = 6 << 2 | INT,
			INT64       = 7 << 2 | INT,
			UINT8       = 8 << 2 | INT,
			UINT16      = 9 << 2 | INT,
			UINT32      = 10 << 2 | INT,
			UINT64      = 11 << 2 | INT,
			BOOL        = 12 << 2,
			STRING      = 13 << 2,
			BINARY      = 14 << 2,
			ARRAY       = 15 << 2,
			OBJECT      = 16 << 2,
			EXTENSION   = 17 << 2
		};
		
		// Array and object typedefs
		using array=std::deque<MsgPack>;
		using object=std::unordered_map<MsgPack,MsgPack>;
		using string=std::string;
		//floats
		using float32=float;
		using float64=double;
		//boolean
		using boolean=bool;
		// Binary and extension typedefs
		using binary=std::vector<uint8_t>;
		using extension=std::tuple<uint8_t,binary>;
		
		//integers
		using int8=int8_t;
		using int16=int16_t;
		using int32=int32_t;
		using int64=int64_t;
		using uint8=uint8_t;
		using uint16=uint16_t;
		using uint32=uint32_t;
		using uint64=uint64_t;
		using int128=__int128;
		
		// Constructors for the various types of JSON value.
		MsgPack();                         // NUL
		MsgPack(std::nullptr_t);           // NUL
//		template<typename T> requires(std::is_fundamental_v<T>)
//		MsgPack(T value);
//		
//		template<typename T> requires(std::is_class_v<T>)
//		MsgPack(const T& value);
//		template<typename T> requires(std::is_class_v<T>)
//		MsgPack(T&& value);
		MsgPack(float32 value);              // FLOAT32
		MsgPack(float64 value);             // FLOAT64
		MsgPack(int8 value);             // INT8
		MsgPack(int16 value);            // INT16
		MsgPack(int32 value);            // INT32
		MsgPack(int64 value);            // INT64
		MsgPack(uint8 value);            // UINT8
		MsgPack(uint16 value);           // UINT16
		MsgPack(uint32 value);           // UINT32
		MsgPack(uint64 value);           // UINT64
		MsgPack(bool value);               // BOOL
		MsgPack(const string &value);      // STRING
		MsgPack(string &&value);           // STRING
		MsgPack(const array &values);      // ARRAY
		MsgPack(array &&values);           // ARRAY
		MsgPack(const object &values);     // OBJECT
		MsgPack(object &&values);          // OBJECT
		MsgPack(const binary &values);     // BINARY
		MsgPack(binary &&values);          // BINARY
		MsgPack(const extension &values);  // EXTENSION
		MsgPack(extension &&values);       // EXTENSION
		
		// Implicit constructor: anything with a to_msgpack() function.
		template <class T> requires requires(T thing){MsgPack(thing.to_msgpack());}
		MsgPack(const T & t) : MsgPack(t.to_msgpack()) {}
		
		// Implicit constructor: map-like objects (std::map, std::unordered_map, etc)
		template <class M> requires
		(
			requires(M map)
			{
				typename M::key_type;
				typename M::mapped_type;
			}&&
			requires(typename M::key_type key){MsgPack(key);}&&
			requires(typename M::mapped_type value){MsgPack(value);}&&
			!std::is_same_v<M,object>
		)
		MsgPack(const M & m) : MsgPack(object(std::begin(m),std::end(m))) {}
		
		// Implicit constructor: vector-like objects (std::list, std::vector, std::set, etc)
		template <class V>requires
		(
			requires(V arr){typename V::value_type;}&&
			requires(typename V::value_type value){MsgPack(value);}&&
			!std::is_same_v<typename binary::value_type, typename V::value_type>&&
			!std::is_same_v<V,array>&&
			!std::is_same_v<V,string>&&
			!std::is_same_v<V,binary>&&
			!std::is_same_v<V,object>
		)
		MsgPack(const V & v):MsgPack(array(std::begin(v),std::end(v))){}
		
		template <class V>requires
		(
			requires(V arr){typename V::value_type;}&&
			requires(typename V::value_type value){MsgPack(value);}&&
			std::is_same_v<typename binary::value_type,typename V::value_type>&&
			!std::is_same_v<V,binary>&&
			!std::is_same_v<V,array>&&
			!std::is_same_v<V,string>&&
			!std::is_same_v<V,object>
		)
		MsgPack(const V & v):MsgPack(binary(std::begin(v),std::end(v))) {}
		
		// This prevents MsgPack(some_pointer) from accidentally producing a bool. Use
		// MsgPack(bool(some_pointer)) if that behavior is desired.
		MsgPack(void *) = delete;
		
		// Accessors
		Type type() const;
		
		bool is_null()      const { return type() == Type::NUL; }
		bool is_boolean()   const { return type() == Type::BOOL; }
		bool is_number()    const { return static_cast<uint8_t>(type())&static_cast<uint8_t>(Type::NUMBER); }
		bool is_float32()   const { return type() == Type::FLOAT32; }
		bool is_float64()   const { return type() == Type::FLOAT64; }
		bool is_int()       const { return (static_cast<uint8_t>(type())&static_cast<uint8_t>(Type::INT))==static_cast<uint8_t>(Type::INT); }
		bool is_int8()      const { return type() == Type::INT8; }
		bool is_int16()     const { return type() == Type::INT16; }
		bool is_int32()     const { return type() == Type::INT32; }
		bool is_int64()     const { return type() == Type::INT64; }
		bool is_uint8()     const { return type() == Type::UINT8; }
		bool is_uint16()    const { return type() == Type::UINT16; }
		bool is_uint32()    const { return type() == Type::UINT32; }
		bool is_uint64()    const { return type() == Type::UINT64; }
		bool is_string()    const { return type() == Type::STRING; }
		bool is_array()     const { return type() == Type::ARRAY; }
		bool is_binary()    const { return type() == Type::BINARY; }
		bool is_object()    const { return type() == Type::OBJECT; }
		bool is_extension() const { return type() == Type::EXTENSION; }
		
		// Return the enclosed value if this is a number, 0 otherwise. Note that msgpack11 does not
		// distinguish between integer and non-integer numbers - number_value() and int_value()
		// can both be applied to a NUMBER-typed object.
		
		template<typename T> requires(!std::is_const_v<T>)
		explicit operator T&();
		template<typename T> requires(!std::is_const_v<T>)
		T& as(){return operator T&();}
		
		// Return the enclosed value if this is a number, throws exception otherwise. Note that msgpack11 does not
		// distinguish between integer and non-integer numbers - number_value() and int_value()
		// can both be applied to a NUMBER-typed object.
		
		template<typename T> requires(std::is_fundamental_v<T>)
		explicit operator T() const;
		template<typename T> requires(std::is_fundamental_v<T>)
		T as() const{return operator T();}
		
		template<typename T> requires(!std::is_fundamental_v<T>)
		explicit operator const T&() const;
		template<typename T> requires(!std::is_fundamental_v<T>)
		const T& as() const{return operator const T&();}
		
		//cast for immutable types
		
		// Return a reference to arr[i] if this is an array, MsgPack() otherwise.
		const MsgPack& operator[](size_t i) const;
		// Return a mutable reference to arr[i] if this is an array, MsgPack() otherwise.
		MsgPack& operator[] (size_t i);
		// Return a reference to obj[key] if this is an object, MsgPack() otherwise.
		const MsgPack& operator[](const MsgPack &key) const;
		// Return a referenct to obj[key] if this is an object and obj[key] exists, creating a new member otherwise.
		MsgPack& operator[](const MsgPack &key);
		
		// Serialize.
		void dump(std::string &out) const
		{
			std::stringstream ss;
			ss<<*this;
			out=ss.str();
		}
		
		std::string dump() const
		{
			std::stringstream ss;
			ss<<*this;
			return ss.str();
		}
		
		friend std::ostream& operator<<(std::ostream& os, const MsgPack& msgpack);
		// Parse. If parse fails, set msgpack to MsgPack() and
		// sets failbit on stream.
		friend std::istream& operator>>(std::istream& is, MsgPack& msgpack);
		
		// Parse. If parse fails, return MsgPack() and assign
		// an error message to err.
		static MsgPack parse(const std::string & in, std::string & err);
		// Parse. If parse fails, return MsgPack(), sets failbit on stream and and
		// assign an error message to err.
		static MsgPack parse(std::istream& is, std::string &err);
		// Parse (without the need to default initialise object first).
		// If parse fails, return MsgPack() and sets failbit on stream.
		static MsgPack parse(std::istream& is);
		static MsgPack parse(const char * in, size_t len, std::string & err)
		{
			if (in)
			{
				return parse(std::string(in,in+len),err);
			}
			else
			{
				err = "null input";
				return nullptr;
			}
		}
		// Parse multiple objects, concatenated or separated by whitespace
		static std::vector<MsgPack> parse_multi(
			const std::string & in,
			std::string::size_type & parser_stop_pos,
			std::string & err);
		
		static inline std::vector<MsgPack> parse_multi(
			const std::string & in,
			std::string & err)
		{
			std::string::size_type parser_stop_pos;
			return parse_multi(in, parser_stop_pos, err);
		}
		
		bool operator== (const MsgPack &rhs) const;
		std::partial_ordering operator<=>(const MsgPack &rhs) const;
		
		bool operator!= (const MsgPack &rhs) const=default;
		bool operator<  (const MsgPack &rhs) const=default;
		bool operator<= (const MsgPack &rhs) const=default;
		bool operator>  (const MsgPack &rhs) const=default;
		bool operator>= (const MsgPack &rhs) const=default;
		
		/* has_shape(types, err)
     *
     * Return true if this is a JSON object and, for each item in types, has a field of
     * the given type. If not, return false and set err to a descriptive message.
     */
		typedef std::initializer_list<std::pair<std::string, Type>> shape;
		bool has_shape(const shape & types, std::string & err) const;
		
	private:
		std::shared_ptr<MsgPackValue> m_ptr;
		friend struct std::hash<MsgPack>;
	};
	
} // namespace msgpack11
