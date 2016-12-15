MRuby::Build.new do |conf|
  toolchain :gcc
  enable_test
  conf.gembox 'default'
  conf.gem '../mruby-nkf'
end

MRuby::Build.new('cxx_abi') do |conf|
  toolchain :gcc
  enable_test
  enable_cxx_abi
  conf.gembox 'default'
  conf.gem '../mruby-nkf'
end
