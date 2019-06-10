define ctf-btr-show-stack
    if (stack_empty($arg0))
        printf "stack is empty!\n"
    else
        set $stack_size = stack_size($arg0)
        set $stack_at = (int) ($stack_size - 1)
        printf "%3s    %10s   %4s    %3s\n", "pos", "base addr", "blen", "idx"

        while ($stack_at >= 0)
            set $stack_entry = (struct stack_entry *) g_ptr_array_index($arg0->entries, $stack_at)

            if ($stack_at == $stack_size - 1)
                printf "%3d    %10p    %3d    %3d  <-- top\n", $stack_at, \
                    $stack_entry->base_class, $stack_entry->base_len, \
                    $stack_entry->index
            else
                printf "%3d    %10p    %3d    %3d\n", $stack_at, \
                    $stack_entry->base_class, $stack_entry->base_len, \
                    $stack_entry->index
            end
            set $stack_at = $stack_at - 1
        end
    end
end
