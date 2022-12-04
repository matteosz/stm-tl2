for i in {0..5}
do
    echo $i;
    python submit.py --uuid Pu5X18RbIXSgddra1hUCbAKhgHnnnFNL ../../360013.zip;
    sleep 60;
done
$BASH