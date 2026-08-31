# legit - Build Your Own Git in C

> Based on [CodeCrafters "Build Your Own Git" Challenge](https://codecrafters.io/challenges/git) - original template by [CodeCrafters](https://codecrafters.io). Rebuilt and extended as `legit`.

[![progress-banner](https://backend.codecrafters.io/progress/git/1f907f09-a1fd-472d-8fa4-562374cef476)](https://app.codecrafters.io/users/plasmaDestroyer?r=2qF)

A small Git implementation in C - `init`, `cat-file`, `hash-object` so far. Started from CodeCrafters template, now standalone.

## Usage

```sh
# build
cmake -B build -S . && cmake --build ./build

# or via wrapper (builds + runs)
./your_program.sh init

# commands
./build/git init
./build/git cat-file -p <sha>
./build/git hash-object -w <file>

# alias for testing
alias legit=/path/to/legit/your_program.sh
mkdir -p /tmp/testing && cd /tmp/testing
legit init
echo "hello world" > test.txt
legit hash-object -w test.txt
legit cat-file -p <sha>
```

## Testing locally

Run in a separate dir so you don't damage this repo's `.git`:

```sh
mkdir -p /tmp/testing && cd /tmp/testing
/path/to/legit/your_program.sh init
```
