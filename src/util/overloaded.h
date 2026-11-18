#ifndef C797C0EE_BBFB_4203_A3EB_F776294F4C97
#define C797C0EE_BBFB_4203_A3EB_F776294F4C97

template <class... Ts>
struct overloaded : Ts... {
  using Ts::operator()...;
};
// explicit deduction guide (not needed as of C++20)
template <class... Ts>
overloaded(Ts...) -> overloaded<Ts...>;

#endif /* C797C0EE_BBFB_4203_A3EB_F776294F4C97 */
