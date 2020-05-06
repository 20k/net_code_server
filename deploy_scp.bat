scp ./bin/LDeploy/crapmud root@192.168.0.55:/home/ncs/crapmud
scp ./script_compile/transpile.js root@192.168.0.55:/home/ncs/script_compile/transpile.js
scp ./deps/secret/source/* root@192.168.0.55:/home/ncs/deps/secret/source
scp ./deps/secret/cert/* root@192.168.0.55:/home/ncs/deps/secret/cert
scp ./deps/secret/akey.ect root@192.168.0.55:/home/ncs/deps/secret/akey.ect
scp ./deps/steamworks_sdk_142/sdk/public/steam/lib/linux64/libsdkencryptedappticket.so root@192.168.0.55:/lib/libsdkencryptedappticket.so
scp ./deps/libs/libsfml-system* root@192.168.0.55:/lib
rem scp ./script_compile/package.json root@192.168.0.55:/home/ncs/package.json
scp /home/james/boost_1_73_0/stage/lib/libboost_fiber.so.1.73.0 root@192.168.0.55:/lib/libboost_fiber.so.1.73.0
scp /home/james/boost_1_73_0/stage/lib/libboost_context.so.1.73.0 root@192.168.0.55:/lib/libboost_context.so.1.73.0