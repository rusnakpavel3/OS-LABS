
R=${1:-100000000}
shift
THREADS=${*:-"1 2 4 6 8 12 16 128 1024"}

measure() {
	for k in 1 2 3; do
		./lab2 "$@" | awk '/^Время/ { print $2 }'
	done | sort -n | sed -n 2p
}

./lab2 -r "$R" > /dev/null   
TS=$(measure -r "$R")

echo "Раундов: $R"
echo
echo "| Число потоков | Время исполнения (мс) | Ускорение | Эффективность |"
echo "|---|---|---|---|"
echo "| последовательная | $TS | — | — |"
for t in $THREADS; do
	TP=$(measure -r "$R" -t "$t")
	awk -v t="$t" -v ts="$TS" -v tp="$TP" \
		'BEGIN { s = ts / tp; printf "| %d | %s | %.2f | %.3f |\n", t, tp, s, s / t }'
done
