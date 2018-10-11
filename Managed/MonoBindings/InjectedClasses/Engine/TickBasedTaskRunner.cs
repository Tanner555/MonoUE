// Copyright (c) Microsoft Corporation.  All Rights Reserved.
// See LICENSE.txt in the plugin root for license information.

using System;
using System.Collections.Generic;
using System.Threading;
using System.Threading.Tasks;

namespace UnrealEngine.Engine
{

    /// <summary>
    /// Runs a task to update an actor, but dispatching the continuations only when its Tick method is called.
    /// </summary>
    public class TickBasedTaskRunner : SynchronizationContext, IDisposable
    {
        CancellationTokenSource cts;
        Func<CancellationToken,Task> creator;
        Task task;
        bool repeat;

        //captures Task continuations via the sync context so they can be dequeued on the tick
        readonly List<KeyValuePair<SendOrPostCallback,object>> callbacks
            = new List<KeyValuePair<SendOrPostCallback,object>>();

        public TickBasedTaskRunner(Func<CancellationToken,Task> creator, bool repeat = false)
        {
            this.creator = creator;
            this.repeat = repeat;
            CreateTask();
        }

        public void Tick()
        {
            if (creator == null)
                throw new ObjectDisposedException("ActorUpdater");

            if (callbacks.Count != 0)
                RunPendingCallbacks();

            if (task != null)
            {
                switch (task.Status)
                {
                    case TaskStatus.RanToCompletion:
                    case TaskStatus.Canceled:
                        //task is done, need to create a new one
                        task = null;
                        break;
                    case TaskStatus.Faulted:
                        //rethrow pending exceptions from the updater
                        //but first clear it so we won't rethrow it next time
                        var x = task;
                        task = null;
                        throw x.Exception;
                    default:
                        //task is still running, let it be
                        return;
                }
            }

            if (repeat)
            {
                if (cts != null)
                    cts.Cancel();
                CreateTask();
            }
        }

        void CreateTask()
        {
            //save and existing sync context and substitute ours
            var oldCtx = SynchronizationContext.Current;
            try
            {
                SynchronizationContext.SetSynchronizationContext(this);
                cts = new CancellationTokenSource();
                task = creator(cts.Token);
            }
            finally
            {
                //restore old sync context
                SynchronizationContext.SetSynchronizationContext(oldCtx);
            }
        }

        public void Dispose()
        {
            //TODO: warn if pending unflushed callbacks?
            cts.Cancel();
            cts = null;
            creator = null;
            task = null;
        }

        public override void Post(SendOrPostCallback d, object state)
        {
            callbacks.Add(new KeyValuePair<SendOrPostCallback, object>(d, state));
        }

        void RunPendingCallbacks()
        {
            //set this as the sync context in case callbacks create more continutations
            var oldCtx = SynchronizationContext.Current;
            SynchronizationContext.SetSynchronizationContext(this);

            foreach (var cb in callbacks)
            {
                //don't think this will ever throw, task will capture exceptions?
                cb.Key(cb.Value);
            }

            //restore old sync context
            SynchronizationContext.SetSynchronizationContext(oldCtx);

            callbacks.Clear();
        }
    }
}
