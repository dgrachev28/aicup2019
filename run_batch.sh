../aicup2019-linux/aicup2019 --config config.json --save-results res.log --save-replay replay.rep &
sleep 1
./versions/aicup2019_2_0 127.0.0.1 31002 &
sleep 1
./cmake-build-debug/aicup2019 127.0.0.1 31001 &