import time

def fib(n):
    if n < 2:
        return n
    return fib(n - 2) + fib(n - 1)

start = time.perf_counter()
for i in range(10):
    fib(35)
end = time.perf_counter()

print("Mean time: ", (end-start)/10)