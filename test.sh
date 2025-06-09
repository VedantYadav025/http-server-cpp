for i in {1..10}; do
  (curl -v http://localhost:4221/echo/hello$i &) 
done
wait

