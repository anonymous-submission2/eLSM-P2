for file in *.cc; do
    mv "$file" "$(basename "$file" .cc).cpp"
done
