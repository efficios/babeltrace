define ctf-notif-iter-show-stack
    if (stack_empty($arg0))
        printf "stack is empty!\n"
    else
        set $stack_size = stack_size($arg0)
        set $stack_at = (int) ($stack_size - 1)
        printf "%3s    %10s    %3s\n", "pos", "base addr", "idx"

        while ($stack_at >= 0)
            set $stack_entry = (struct stack_entry *) g_ptr_array_index($arg0->entries, $stack_at)

            if ($stack_at == $stack_size - 1)
                printf "%3d    %10p    %3d  <-- top\n", $stack_at, \
                    $stack_entry->base, $stack_entry->index
            else
                printf "%3d    %10p    %3d\n", $stack_at, \
                    $stack_entry->base, $stack_entry->index
            end
            set $stack_at = $stack_at - 1
        end
    end
end
