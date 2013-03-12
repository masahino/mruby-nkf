assert('NKF guess 1') do
  EUC_STR = "\xa5\xaa\xa5\xd6\xa5\xb8\xa5\xa7\xa5\xaf\xa5\xc8\xbb\xd8\xb8\xfe\
\xa5\xb9\xa5\xaf\xa5\xea\xa5\xd7\xa5\xc8\xb8\xc0\xb8\xec\
Ruby"

  str_euc = EUC_STR
  str_jis = NKF.nkf('-j', str_euc)
  ::NKF::JIS ==  NKF.guess(str_jis)
  ::NKF::EUC ==  NKF.guess(str_euc)
end

