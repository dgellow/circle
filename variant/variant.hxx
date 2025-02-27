#if !defined(__circle_build__) || __circle_build__ < 147
#error Must compile with Circle build 147 or later
#endif

#if !defined(__cplusplus) || __cplusplus < 202002
#error Must compile with -std=c++20
#endif

#pragma once

#include <new>
#include <utility>
#include <compare>

namespace circle {

// [variant.bad.access]
class bad_variant_access : public std::exception { 
  const char* message;
public:
  bad_variant_access(const char* message) noexcept : message(message) { }
  const char* what() const noexcept override { return message; }
};

inline constexpr size_t variant_npos = -1;

template<class... Types>
class variant;

template<typename T>
concept is_variant = T.template == variant;

template<class T>
struct variant_size;

template<class T>
struct variant_size<const T> : variant_size<T> { };

template<class... Types>
struct variant_size<variant<Types...> > : 
  std::integral_constant<size_t, sizeof...(Types)> { };

template<typename T>
inline constexpr size_t variant_size_v = variant_size<T>::value;

template<size_t I, typename T>
struct variant_alternative;

template<size_t I, typename... Types>
struct variant_alternative<I, variant<Types...>> {
  using type = Types...[I];
};

template<size_t I, typename T>
using variant_alternative_t = typename variant_alternative<I, T>::type;

template<size_t I, typename T>
struct variant_alternative<I, const T> { 
  using type = const variant_alternative_t<I, T>; 
};

template<class... Types>
class variant {
  static_assert(sizeof...(Types) > 0 && sizeof...(Types) < 256);

  static constexpr bool copy_constructible = 
    (... && std::is_copy_constructible_v<Types>);
  static constexpr bool move_constructible =
    (... && std::is_move_constructible_v<Types>);
  static constexpr bool copy_assignable = 
    (... && std::is_copy_assignable_v<Types>);
  static constexpr bool move_assignable =
    (... && std::is_move_assignable_v<Types>);

  static constexpr bool trivially_copy_constructible =
    (... && std::is_trivially_copy_constructible_v<Types>);
  static constexpr bool trivially_move_constructible =
    (... && std::is_trivially_move_constructible_v<Types>);
  static constexpr bool trivially_copy_assignable =
    (... && std::is_trivially_copy_assignable_v<Types>);
  static constexpr bool trivially_move_assignable = 
    (... && std::is_trivially_move_assignable_v<Types>);
  static constexpr bool trivially_destructible = 
    (... && std::is_trivially_destructible_v<Types>);

  static constexpr bool nothrow_copy_constructible =
    (... && std::is_nothrow_copy_constructible_v<Types>);
  static constexpr bool nothrow_move_constructible =
    (... && std::is_nothrow_move_constructible_v<Types>);
  static constexpr bool nothrow_copy_assignable =
    (... && std::is_nothrow_copy_assignable_v<Types>);
  static constexpr bool nothrow_move_assignable = 
    (... && std::is_nothrow_move_assignable_v<Types>);
  static constexpr bool nothrow_swappable =
    (... && std::is_nothrow_swappable_v<Types>);

  // If all member types are nothrow assignable, we can't get into a 
  // valueless state.
  static constexpr bool never_valueless = nothrow_copy_assignable &&
    nothrow_move_assignable;

  // [variant.assign]
  // This operator is defined as deleted unless is_copy_constructible_v<Ti>
  // && is_copy_assignable_v<Ti> is true for all i.
  static constexpr bool copy_assign_deleted = !copy_constructible ||
    !copy_assignable;

  // If is_trivially_copy_constructible_v<Ti> && 
  // is_trivially_copy_assignable_v<Ti> && is_trivially_destructible_v<Ti>
  // is true for all i, this assignment operator is trivial.
  static constexpr bool copy_assign_trivial = trivially_copy_constructible &&
    trivially_copy_assignable && trivially_destructible;

  // If is_trivially_move_constructible_v<Ti> && 
  // is_trivially_move_assignable<Ti> && is_trivially_destructible_v<Ti>
  // is true for all i, this assignment operator is trivial.
  static constexpr bool move_assign_trivial = trivially_move_constructible &&
    trivially_move_assignable && trivially_destructible;

  // The exception specification is equivalent to 
  // is_nothrow_move_constructible_v<Ti> && is_nothrow_move_assignable_v<Ti>
  // for all i.
  static constexpr bool move_assign_nothrow = nothrow_move_constructible &&
    nothrow_move_assignable;

  // [variant.swap]
  // The exception specification is equivalent to the logical AND of
  // is_nothrow_move_constructible_v<Ti> && is_nothrow_swappable<Ti> for all i.
  static constexpr bool swap_nothrow = nothrow_move_constructible && 
    nothrow_swappable;

  template<typename T>
  static constexpr bool is_single_usage = 1 == (... + (T == Types));

  template<typename T>
  static constexpr size_t find_index = T == Types ...?? int... : -1;

  union {
    Types ...m;
  };

  uint8_t _index = variant_npos;

public:
  //////////////////////////////////////////////////////////////////////////////
  // [variant.ctor]

  // Default ctor.
  constexpr variant() 
  noexcept(std::is_nothrow_default_constructible_v<Types...[0]>) 
  requires(std::is_default_constructible_v<Types...[0]>) : 
    m...[0](), _index(0) { }

  // Copy ctors.
  constexpr variant(const variant& w)
  requires(trivially_copy_constructible) = default;

  constexpr variant(const variant& w)
  noexcept(nothrow_copy_constructible) 
  requires(copy_constructible && !trivially_copy_constructible) {
    if(!w.valueless_by_exception()) {
      int... == w._index ...? 
        (void)new(&m) Types(w. ...m) : 
        __builtin_unreachable();
      _index = w._index;
    }
  }

  constexpr variant(const variant& w)
  requires(!copy_constructible) = delete;

  // Move ctors.
  constexpr variant(variant&& w) 
  requires(trivially_move_constructible) = default;

  constexpr variant(variant&& w)
  noexcept(nothrow_move_constructible)
  requires(move_constructible && !trivially_move_constructible) {
    if(!w.valueless_by_exception()) {
      int... == w._index ...? 
        (void)new(&m) Types(std::move(w. ...m)) : 
        __builtin_unreachable();
      _index = w._index;
    }
  }

  // Converting constructor.
  // Let Tj be a type that is determined as follows: build an imaginary
  // function FUN(Ti) for each alternative type Ti for which 
  // Ti x[] = {std::forward<T>(t)}; is well-formed for some invented 
  // variable x. The overload FUN(Tj) selected by overload resolution for
  // the expression FUN(std::forward<T>(t)) defines the alternative Tj
  // which is the type of the contained value after construction.
  template<typename T, int j = __preferred_copy_init(T, Types...)>
  requires(
    -1 != j &&
    T.remove_cvref != variant && 
    T.template != std::in_place_type_t &&
    T.template != std::in_place_index_t && 
    std::is_constructible_v<Types...[j], T>
  )
  constexpr variant(T&& arg) 
    noexcept(std::is_nothrow_constructible_v<Types...[j], T>) :
    m...[j](std::forward<T>(arg)), _index(j) { }

  // Construct a specific type.
  template<class T, class... Args, size_t j = find_index<T> >
  requires(is_single_usage<T> && std::is_constructible_v<T, Args...>) 
  explicit constexpr variant(std::in_place_type_t<T>, Args&&... args)
  noexcept(std::is_nothrow_constructible_v<T, Args...>):
    m...[j](std::forward<Args>(args)), _index(j) { }

  template<class T, class U, class... Args, size_t j = find_index<T> >
  requires(is_single_usage<T> && 
    std::is_constructible_v<T, std::initializer_list<U>, Args...>)
  explicit constexpr variant(std::in_place_type_t<T>, 
    std::initializer_list<U> il, Args&&... args)
  noexcept(std::is_nothrow_constructible_v<T, std::initializer_list<U>, Args...>) :
    m...[j](il, std::forward<Args>(args)...), _index(j) { }

  // Construct at a specific index.
  template<size_t I, typename... Args>
  requires(
    (I < sizeof...(Types)) && 
    std::is_constructible_v<Types...[I], Args...>
  )
  explicit constexpr variant(std::in_place_index_t<I>, Args&&... args)
    noexcept(std::is_nothrow_constructible_v<Types...[I], Args...>) :
    m...[I](std::forward<Args>(args)...), _index(I) { }

  template<size_t I, class U, typename... Args>
  requires(
    (I < sizeof...(Types)) && 
    std::is_constructible_v<Types...[I], std::initializer_list<U>, Args...>
  )
  explicit constexpr variant(std::in_place_index_t<I>, 
    std::initializer_list<U> il, Args&&... args)
    noexcept(std::is_nothrow_constructible_v<Types...[I], 
      std::initializer_list<U>, Args...>) :
    m...[I](std::forward<Args>(args)...), _index(I) { }

  //////////////////////////////////////////////////////////////////////////////
  // [variant.dtor]
  // Conditionally-trivial destructor.

  constexpr ~variant() requires(trivially_destructible) = default;
  constexpr ~variant() requires(!trivially_destructible) { reset(); }

  //////////////////////////////////////////////////////////////////////////////
  // [variant.assign]

  constexpr variant& operator=(const variant& rhs) 
  requires(copy_assign_deleted) = delete;

  constexpr variant& operator=(const variant& rhs) 
  requires(copy_assign_trivial) = default;

  constexpr variant& operator=(const variant& rhs) 
  requires(!copy_assign_deleted && !copy_assign_trivial) {
    if(!valueless_by_exception() || !rhs.valueless_by_exception()) {
      // If neither *this nor rhs holds a value, there is no effect.

      if(rhs.valueless_by_exception()) {
        // Otherwise, if *this holds a value but rhs does not, destroys the
        // value contained in *this and sets *this to not hold a value.      
        reset();

      } else if(_index == rhs._index) {
        // Otherwise, if index() == j, assigns the value contained in rhs
        // to the value contained in *this.
        int... == _index ...? (void)(m = rhs. ...m) : __builtin_unreachable();

      } else {
        int... == rhs._index ...? 
          std::is_nothrow_copy_constructible_v<Types> ||
          !std::is_nothrow_move_constructible_v<Types> ?? 
            // Otherwise, if is_nothrow_copy_constructible<Tj> is true or 
            // is_nothrow_move_constructible_v<Tj> is false, 
            // emplace<j>(get<j>(rhs)).
            (void)emplace<int...>(rhs. ...m) :

            // Otherwise, equivalent to operator=(variant(rhs))
            (void)operator=(variant(rhs. ...m)) :
          __builtin_unreachable();
      }
    }

    return *this;
  }

  constexpr variant& operator=(variant&& rhs) 
  requires(move_assign_trivial) = default;

  constexpr variant& operator=(variant&& rhs) 
  noexcept(move_assign_nothrow) requires(!move_assign_trivial) {
    if(!valueless_by_exception() || !rhs.valueless_by_exception()) {
      // If neither *this nor rhs holds a value, there is no effect.
 
      if(rhs.valueless_by_exception()) {
        // Otherwise, if *this holds a value but rhs does not, destroy the
        // value contained in *this and sets *this to not hold a value.
        reset();
 
      } else if(_index == rhs._index) {
        // Otherwise, if index() == j, assign get<j>(std::move(rhs)) to the
        // value contained in *this.
        int... == _index ...? 
          (void)(m = std::move(rhs. ...m)) : 
          __builtin_unreachable();
 
      } else {
        // Otherwise, equivalent to emplace<j>(get<j>(std::move(rhs))).
        reset();
        int... == rhs._index ...? 
          (void)new (&m) Types(std::move(rhs. ...m)) :
          __builtin_unreachable();
        _index = rhs._index;
      }
    }
    return *this;
  }

  // Converting assignment.
  // Let Tj be a type that is determined as follows: build an imaginary 
  // function FUN(Ti) for each alternative type Ti for which 
  // Ti x[] = {std::forward<T>(t)]}; is well-formed for some invented
  // variable x. The overload FUN(Tj) selected by overload resolution for
  // the expression FUN(std::forward<T>(t)) defines the alternative Tj which
  // is the type of the contained value after assignment.
  template<class T, size_t j = __preferred_assignment(T&&, Types...)>
  requires(T.remove_cvref != variant && -1 != j &&
    std::is_constructible_v<Types...[j], T>)
  constexpr variant& operator=(T&& t) 
  noexcept(std::is_nothrow_assignable_v<Types...[j], T> &&
    std::is_nothrow_constructible_v<Types...[j], T>) {
 
    if(_index == j) {
      // If *this holds Tj, assigns std::forward<T>(t) to the value contained
      // in *this.
      m...[j] = std::forward<T>(t);
 
    } else if constexpr(std::is_nothrow_constructible_v<Types...[j], T> ||
      !std::is_nothrow_move_constructible_v<Types...[j]>) {
 
      // Otherwise, if is_nothrow_constructible_v<Tj, T> || 
      // !is_nothrow_move_constructible_v<Tj> is true, equivalent to
      // emplace<j>(Tj(std::forward<T>(t))).
      reset();
      new(&m...[j]) Types...[j](std::forward<T>(t));
      _index = j;
 
    } else {
      // Otherwise, equivalent to emplace<j>(Tj(std::forward<T>(t))).
      Types...[j] temp(std::forward<T>(t));
      reset();
      new(&m...[j]) Types...[j](std::move(temp));
      _index = j;
    }
 
    return *this;
  }
 
  // [variant.mod]
  template<class T, class... Args, size_t j = find_index<T> >
  requires(-1 != j && std::is_constructible_v<T, Args...> && is_single_usage<T>)
  constexpr T& emplace(Args&&... args) 
  noexcept(std::is_nothrow_constructible_v<T, Args...>) {
    return emplace<j>(std::forward<Args>(args)...);
  }

  template<class T, class U, class... Args, size_t j = find_index<T> >
  requires(-1 != j && is_single_usage<T> && 
    std::is_constructible_v<T, std::initializer_list<U>&, Args...>)
  constexpr T& emplace(std::initializer_list<U> il, Args&&... args)
  noexcept(std::is_nothrow_constructible_v<T, std::initializer_list<U>, Args...>) {
    return emplace<j>(il, std::forward<Args>(args)...);
  }

  template<size_t I, class... Args>
  requires(std::is_constructible_v<Types...[I], Args...>)
  constexpr Types...[I]& emplace(Args&&... args) 
  noexcept(std::is_nothrow_constructible_v<Types...[I], Args...>) {
    reset();
    new (&m...[I]) Types...[I](std::forward<Args>(args)...);
    _index = I;
    return m...[I];
  }

  template<size_t I, class U, class... Args>
  requires(std::is_constructible_v<Types...[I], std::initializer_list<U>&, Args...>)
  constexpr Types...[I]& emplace(std::initializer_list<U> il, Args&&... args)
  noexcept(std::is_nothrow_constructible_v<Types...[I], std::initializer_list<U>, Args...>) {
    reset();
    new (&m...[I]) Types...[I](il, std::forward<Args>(args)...);
    _index = I;
    return m...[I];
  }

  // [variant.status]
  constexpr bool valueless_by_exception() const noexcept {
    return !never_valueless && variant_npos == index();
  }
  constexpr size_t index() const noexcept {
    return _index;
  }

  // [variant.swap]
  constexpr void swap(variant& rhs) noexcept(swap_nothrow)
  requires(move_constructible) {
    if(!valueless_by_exception() || !rhs.valueless_by_exception()) {
      // If valueless_by_exception() && rhs.valueless_by_exception() no effect.

      if(_index == rhs._index) {
        // Otherwise, if index() == rhs.index(), calls 
        // swap(get<i>(*this), get<i>(rhs)) where i is index.
        using std::swap;
        int... == _index ...? swap(m, rhs. ...m) : __builtin_unreachable();
        swap(_index, rhs._index);

      } else {
        // This is a slow op. Use a variant to hold a temporary for the
        // lhs active variant member.
        variant a_temp = std::move(*this);
        *this = std::move(rhs);
        rhs = std::move(a_temp);
      }
    }
  }

  // Helper functions exposed for convenience.
  constexpr void reset() noexcept {
    if(_index != variant_npos) {
      _index == int... ...? m.~Types() : __builtin_unreachable();
      _index = variant_npos;        // set to valueless by exception.
    }
  }

  template<size_t I, typename Self>
  constexpr auto&& get(this Self&& self) noexcept {
    static_assert(I < sizeof...(Types));
    return std::forward<Self>(self). ...m...[I];
  }
};

// [variant.get]
template<class T, class... Types>
constexpr bool holds_alternative(const variant<Types...>& v) noexcept {
  static_assert(1 == (... + (Types == T)));
  return T == Types ...?? int... == v.index() : __builtin_unreachable();
}

template<size_t I, is_variant Var>
constexpr auto&& get(Var&& v) {
  return I == v.index() ? 
    std::forward<Var>(v).template get<I>() : 
    throw bad_variant_access("variant get has valueless index");
}

template<typename T, is_variant Var>
constexpr auto&& get(Var&& v) {
  static_assert(1 == (... + (Var.type_args == T)));
  constexpr size_t I = Var.type_args == T ...?? int... : -1;
  return I == v.index() ? 
    std::forward<Var>(v).template get<I>() : 
    throw bad_variant_access("variant get has valueless index");
}

template<size_t I, is_variant Var>
constexpr auto get_if(Var* v) {
  return I == v->index() ?
    v->template get<I>() : 
    nullptr;
}

template<typename T, is_variant Var>
constexpr auto get_if(Var* v) {
  static_assert(1 == (... + (Var.type_args == T)));
  constexpr size_t I = Var.type_args == T ...?? int... : -1;
  return I == v.index() ? 
    v->template get<I>() : 
    nullptr;
}

// [variant.relops]
template<class... Types>
constexpr bool operator==(const variant<Types...>& v, 
  const variant<Types...>& w) {

  // Mandates: get<i>(v) == get<i>(w) is a valid expression that is 
  // convertible to bool, for all i.
  static_assert(requires{ (bool)(get<int...>(v) == get<int...>(w)); }, 
    Types.string + " has no operator==")...;

  // Returns: 
  //  If v.index() != w.index(), false; 
  //  otherwise, if v.valueless_by_exception(), true;
  //  otherwise, get<i>(v) == get<i>(w) with i being v.index().
  return v.index() != w.index() ? false :
    v.valueless_by_exception() ? true :
    int...(sizeof...(Types)) == v.index() ...? 
      v.template get<int...>() == w.template get<int...>() :
      __builtin_unreachable();
}

template<class... Types>
constexpr bool operator!=(const variant<Types...>& v, 
  const variant<Types...>& w) {

  static_assert(requires{ (bool)(get<int...>(v) != get<int...>(w)); }, 
    Types.string + " has no operator!=")...;

  // Returns:
  //   If v.index() != w.index(), true;
  //   otherwise, if v.valueless_by_exception(), false;
  //   otherwise get<i>(v) != get<i>(w) with i being v.index().
  return v.index() != w.index() ? true :
    v.valueless_by_exception() ? false :
    int...(sizeof...(Types)) == v.index() ...? 
      v.template get<int...>() != w.template get<int...>() :
      __builtin_unreachable();
}

template<class... Types>
constexpr bool operator<(const variant<Types...>& v, 
  const variant<Types...>& w) {

  static_assert(requires{ (bool)(get<int...>(v) < get<int...>(w)); }, 
    Types.string + " has no operator<")...;

  // Returns:
  //  If w.valueless_by_exception(), false;
  //  otherwise if v.valueless_by_exception(), true;
  //  otherwise if v.index() < w.index(), true;
  //  otherwise if v.index() > w.index(), false;
  //  otherwise get<i>(v) < get<i>(w) with i being v.index().
  return w.valueless_by_exception() ? false :
    v.valueless_by_exception() ? true :
    v.index() < w.index() ? true :
    v.index() > w.index() ? false :
    int...(sizeof...(Types)) == v.index() ...? 
      v.template get<int...>() < w.template get<int...>() :
      __builtin_unreachable();
}

template<class... Types>
constexpr bool operator>(const variant<Types...>& v, 
  const variant<Types...>& w) {

  static_assert(requires{ (bool)(get<int...>(v) > get<int...>(w)); }, 
    Types.string + " has no operator>")...;

  // Returns:
  //   If v.valueless_by_exception(), false;
  //   otherwise if w.valueless_by_exception(), true;
  //   otherwise, if v.index() > w.index(), true;
  //   otherwise, if v.index() < w.index(), false;
  //   otherwise get<i>(v) > get<i>(w) with i being v.index().
  return v.valueless_by_exception() ? false :
    w.valueless_by_exception() ? true :
    v.index() > w.index() ? true :
    v.index() < w.index() ? false :
    int...(sizeof...(Types)) == v.index() ...? 
      v.template get<int...>() > w.template get<int...>() :
      __builtin_unreachable();
}

template<class... Types>
constexpr bool operator<=(const variant<Types...>& v, 
  const variant<Types...>& w) {

  static_assert(requires{ (bool)(get<int...>(v) <= get<int...>(w)); }, 
    Types.string + " has no operator<=")...;

  // Returns:
  //  If v.valueless_by_exception(), true;
  //  otherwise if w.valueless_by_exception(), false;
  //  otherwise if v.index() < w.index(), true;
  //  otherwise if v.index() > w.index(), false;
  //  otherwise get<i>(v) <= get<i>(w) with i being v.index().
  return v.valueless_by_exception() ? true :
    w.valueless_by_exception() ? false :
    v.index() < w.index() ? true :
    v.index() > w.index() ? false :
    int...(sizeof...(Types)) == v.index() ...? 
      v.template get<int...>() <= w.template get<int...>() :
      __builtin_unreachable();
}

template<class... Types>
constexpr bool operator>=(const variant<Types...>& v, 
  const variant<Types...>& w) {

  static_assert(requires{ (bool)(get<int...>(v) >= get<int...>(w)); }, 
    Types.string + " has no operator>=")...;

  // Returns:
  //   If w.valueless_by_exception(), true;
  //   otherwise if v.valueless_by_exception(), false;
  //   otherwise, if v.index() > w.index(), true;
  //   otherwise, if v.index() < w.index(), false;
  //   otherwise get<i>(v) >= get<i>(w) with i being v.index().
  return w.valueless_by_exception() ? true :
    v.valueless_by_exception() ? false :
    v.index() > w.index() ? true :
    v.index() < w.index() ? false :
    int...(sizeof...(Types)) == v.index() ...? 
      v.template get<int...>() >= w.template get<int...>() :
      __builtin_unreachable();
}

template<class... Types> requires (... && std::three_way_comparable<Types>)
constexpr std::common_comparison_category_t<std::compare_three_way_result_t<Types>...>
operator<=>(const variant<Types...>& v, const variant<Types...>& w) {
  if(v.valueless_by_exception() && w.valueless_by_exception())
    return std::strong_ordering::equal;
  else if(v.valueless_by_exception())
    return std::strong_ordering::less;
  else if(w.valueless_by_exception())
    return std::strong_ordering::greater;
  else if(auto c = v.index() <=> w.index(); c != 0)
    return c;
  else 
    return int...(sizeof...(Types)) == v.index() ...? 
      get<int...>(v) <=> get<int...>(w) :
      __builtin_unreachable();
}

// [variant.visit]
template <class Visitor, class... Variants>
constexpr decltype(auto) visit(Visitor&& vis, Variants&&... vars) {
  if((... || vars.valueless_by_exception()))
    throw bad_variant_access("variant visit has valueless index");

  return __visit<variant_size_v<Variants.remove_reference>...>(
    vis(std::forward<Variants>(vars).template get<indices>()...),
    vars.index()...
  );  
}

template <class R, class Visitor, class... Variants>
constexpr R visit(Visitor&& vis, Variants&&... vars) {
  if((... || vars.valueless_by_exception()))
    throw bad_variant_access("variant visit has valueless index");

  return __visit_r<R, variant_size_v<Variants.remove_reference>...>(
    vis(std::forward<Variants>(vars).template get<indices>()...),
    vars.index()...
  );
}

// [variant.monostate]
class monostate { };

// [variant.monostate.relops]
constexpr bool operator==(monostate, monostate) { return true; }
constexpr std::strong_ordering operator<=>(monostate, monostate) {
  return std::strong_ordering::equal;
}

// [variant.specalg]
template<class... Types>
constexpr void swap(variant<Types...>& v, variant<Types...>& w) 
noexcept(noexcept(v.swap(w))) {
  v.swap(w);
}

} // namespace circle
